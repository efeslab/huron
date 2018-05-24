#include <cassert>
#include <cstdint>
#include <cstdio>
#include <vector>
#include <map>
#include <dlfcn.h>
#include <mutex>

#include "CustomAllocator.h"
#include "MemArith.h"
#include "xthread.h"
#include "LoggingThread.h"
#include "GetGlobal.h"
#include "CacheLine.cpp"

extern "C" {

__thread Thread *current;

void *globalStart, *globalEnd;
void *heapStart = (void *) ((uintptr_t) 1 << 63), *heapEnd;

void initializer(void) __attribute__((constructor));
void finalizer(void) __attribute__((destructor));
void *malloc(size_t size) noexcept;
// void free(void *ptr);
void handle_access(uintptr_t addr, uint64_t func_id, uint64_t inst_id,
                   size_t size, bool is_write);

void store_16bytes(uintptr_t addr, uint64_t func_id, uint64_t inst_id) {
    handle_access(addr, func_id, inst_id, 16, true);
}
void store_8bytes(uintptr_t addr, uint64_t func_id, uint64_t inst_id) {
    handle_access(addr, func_id, inst_id, 8, true);
}
void store_4bytes(uintptr_t addr, uint64_t func_id, uint64_t inst_id) {
    handle_access(addr, func_id, inst_id, 4, true);
}
void store_2bytes(uintptr_t addr, uint64_t func_id, uint64_t inst_id) {
    handle_access(addr, func_id, inst_id, 2, true);
}
void store_1bytes(uintptr_t addr, uint64_t func_id, uint64_t inst_id) {
    handle_access(addr, func_id, inst_id, 1, true);
}
void load_16bytes(uintptr_t addr, uint64_t func_id, uint64_t inst_id) {
    handle_access(addr, func_id, inst_id, 16, false);
}
void load_8bytes(uintptr_t addr, uint64_t func_id, uint64_t inst_id) {
    handle_access(addr, func_id, inst_id, 8, false);
}
void load_4bytes(uintptr_t addr, uint64_t func_id, uint64_t inst_id) {
    handle_access(addr, func_id, inst_id, 4, false);
}
void load_2bytes(uintptr_t addr, uint64_t func_id, uint64_t inst_id) {
    handle_access(addr, func_id, inst_id, 2, false);
}
void load_1bytes(uintptr_t addr, uint64_t func_id, uint64_t inst_id) {
    handle_access(addr, func_id, inst_id, 1, false);
}
}

// typedef std::map<uintptr_t, CacheInfo> addr_cache_map_t;
typedef std::map<uintptr_t, size_t> malloc_size_map_t;
typedef std::map<uintptr_t, CacheLine*> CacheLineBitmap;
CacheLineBitmap allCacheLineInfo;
std::mutex allCacheLineInfoLock;

class MallocHookDeactivator {
public:
    MallocHookDeactivator() noexcept { current->malloc_hook_active = false; }

    ~MallocHookDeactivator() noexcept { current->malloc_hook_active = true; }
};

// This is an (ordered) map because we need to query lower bound for incoming access addresses.
malloc_size_map_t malloc_sizes_in_word;

void initializer(void) {
#ifdef DEBUG
    printf("Initializing...\n");
#endif
    xthread::getInstance().initialize();
    getGlobalRegion(&globalStart, &globalEnd);
    printf("globalStart = %p, globalEnd = %p\n", globalStart, globalEnd);
    current->malloc_hook_active = true;
}

void finalizer(void) {
#ifdef DEBUG
    printf("Finalizing...\n");
    for (auto &p : malloc_sizes_in_word)
        printf("alloc: %p, %lu words\n", (void *) p.first, p.second);
#endif
    xthread::getInstance().flush_all_thread_logs();
}

// void alloc_insert_cachelines(void *start, void *end, addr_cache_map_t &map) {}

extern void *__libc_malloc(size_t size);

extern void __libc_free(void *ptr);

void *my_malloc_hook(size_t size, const void *caller) {
    // RAII deactivate malloc hook so that we can use malloc below.
    MallocHookDeactivator deactiv;
    void *alloced = malloc(size);
    heapStart = std::min(heapStart, alloced);
    heapEnd = std::max(heapEnd, alloced + size);
    // // Round up `size` to whole number of words.
    // size_t size_round = round_up_size(size);
    // // Reserve the space of a WordInfo for each word. The space on the tail
    // // will be used as a WordInfo array.
    // void *alloced = malloc(
    //     (size_round >> word_size_power) * (word_size + 0) // sizeof(WordInfo))
    // );
#ifdef DEBUG
    printf("malloc(%lu) called from %p returns %p\n", size, caller, alloced);
    // printf("heapStart = %p, heapEnd = %p\n", heapStart, heapEnd);
#endif
    // Record that range of address.
    // auto alloced_lu = (uintptr_t)alloced;
    // // Ensure word alignment of initial addr.
    // assert(is_word_aligned(alloced_lu));
    // malloc_sizes_in_word.emplace(alloced_lu, size_round >> word_size_power);

    return alloced;
}

void *malloc(size_t size) noexcept {
    void *caller = __builtin_return_address(0);
    if (current && current->malloc_hook_active)
        return my_malloc_hook(size, caller);
    return __libc_malloc(size);
}

// void my_free_hook(void *ptr, const void *caller) {
//     // RAII deactivate malloc hook so that we can use free below.
//     MallocHookDeactivator deactiv;
//     free(ptr);
// #ifdef DEBUG
//     printf("free(%p) called from %p\n", ptr, caller);
// #endif
// }

// void free(void *ptr) {
//     void *caller = __builtin_return_address(0);
//     // if (current && current->malloc_hook_active)
//         // return my_free_hook(ptr, caller);
//     return __libc_free(ptr);
// }

void handle_access(uintptr_t addr, uint64_t func_id, uint64_t inst_id,
                   size_t size, bool is_write) {
    MallocHookDeactivator deactiv;
    auto addr_ptr = (void *) addr;
    if (
            (addr_ptr < heapStart || addr_ptr >= heapEnd) &&    // not heap object
            (addr_ptr < globalStart || addr_ptr >= globalEnd)   // not global object
            )
        return;
    uintptr_t cacheLineId = (addr >> 6);
    auto found = allCacheLineInfo.find(cacheLineId);
    bool isInstrumented;
    CacheLine *cl_ptr;
    if (found == allCacheLineInfo.end()) {
        /*CacheLine cl;
        if (is_write) cl.store(getThreadIndex());
        else cl.load(getThreadIndex());*/
        cl_ptr = new CacheLine;
        allCacheLineInfoLock.lock();
        allCacheLineInfo.emplace(cacheLineId, cl_ptr);
        allCacheLineInfoLock.unlock();
    }
    else
        cl_ptr = found->second;
    isInstrumented = is_write ? cl_ptr->store(getThreadIndex()) : cl_ptr->load(getThreadIndex());
    if (isInstrumented)
        current->log_load_store(addr, (uint16_t) func_id, (uint16_t) inst_id, (uint16_t) size, is_write);
}

// Intercept the pthread_create function.
int pthread_create(pthread_t *tid, const pthread_attr_t *attr,
                   void *(*start_routine)(void *), void *arg) {
    MallocHookDeactivator deactiv;
    int res = xthread::getInstance().thread_create(tid, attr, start_routine, arg);
    current->malloc_hook_active = true;
    return res;
}
