#include <cstdio>
#include <vector>
#include <mutex>
#include <unordered_map>

#include "MemArith.h"
#include "xthread.h"
#include "GetGlobal.h"
#include "CacheLine.h"

extern "C" {

void *globalStart, *globalEnd;
void *heapStart = (void *) ((uintptr_t) 1 << 63), *heapEnd;

void initializer(void) __attribute__((constructor));
void finalizer(void) __attribute__((destructor));
void *malloc(size_t size) noexcept;
void *__libc_malloc(size_t size);
void __libc_free(void *ptr);
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

typedef std::unordered_map<uintptr_t, CacheLine *> CacheLineBitmap;
CacheLineBitmap allCacheLineInfo;
std::mutex allCacheLineInfoLock;

MallocInfo malloc_sizes;

class MallocHookDeactivator {
public:
    MallocHookDeactivator() noexcept { current->malloc_hook_active = false; }

    ~MallocHookDeactivator() noexcept { current->malloc_hook_active = true; }
};

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
    // RAII deactivate malloc hook so that we can use printf (and malloc) below.
    MallocHookDeactivator deactiv;
#ifdef DEBUG
    printf("Finalizing...\n");
    malloc_sizes.dump();
#endif
    for (const auto &p: allCacheLineInfo)
        delete p.second;
    xthread::getInstance().flush_all_thread_logs();
    malloc_sizes.dump();
}

void *my_malloc_hook(size_t size, const void *caller) {
    // RAII deactivate malloc hook so that we can use malloc below.
    MallocHookDeactivator deactiv;
    void *alloced = malloc(size);
    //mallocStarts.push_back(alloced);
    //mallocOffsets.push_back(size);
    heapStart = std::min(heapStart, alloced);
    heapEnd = std::max(heapEnd, alloced + size);
#ifdef DEBUG
    // printf("malloc(%lu) called from %p returns %p\n", size, caller, alloced);
    // printf("heapStart = %p, heapEnd = %p\n", heapStart, heapEnd);
#endif
    // Record that range of address.
    if (getThreadIndex() == 0)
        malloc_sizes.insert((uintptr_t) alloced, size);
    return alloced;
}

void *malloc(size_t size) noexcept {
    void *caller = __builtin_return_address(0);
    if (current && current->malloc_hook_active)
        return my_malloc_hook(size, caller);
    return __libc_malloc(size);
}


void my_free_hook(void *ptr, const void *caller) {
    // RAII deactivate malloc hook so that we can use free below.
    MallocHookDeactivator deactiv;
    free(ptr);
#ifdef DEBUG
    printf("free(%p) called from %p\n", ptr, caller);
#endif
}

void free(void *ptr) {
    void *caller = __builtin_return_address(0);
    // if (current && current->malloc_hook_active)
    // return my_free_hook(ptr, caller);
    return __libc_free(ptr);
}

void handle_access(uintptr_t addr, uint64_t func_id, uint64_t inst_id,
                   size_t size, bool is_write) {
    MallocHookDeactivator deactiv;
    auto addr_ptr = (void *) addr;
    bool is_heap = (addr_ptr >= heapStart && addr_ptr < heapEnd);
    bool is_global = (addr_ptr >= globalStart && addr_ptr < globalEnd);
    if (!is_heap && !is_global)
        return;
    uintptr_t cacheLineId = (addr >> 6);
    auto found = allCacheLineInfo.find(cacheLineId);
    bool isInstrumented;
    CacheLine *cl_ptr;
    if (found == allCacheLineInfo.end()) {
        cl_ptr = new CacheLine;
        allCacheLineInfoLock.lock();
        allCacheLineInfo.emplace(cacheLineId, cl_ptr);
        allCacheLineInfoLock.unlock();
    } else
        cl_ptr = found->second;
    isInstrumented = is_write ? cl_ptr->store(getThreadIndex()) : cl_ptr->load(getThreadIndex());
    if (isInstrumented) {
        RWRecord rec;
        if (is_heap) {
            try {
                MallocIdSize id_offset = malloc_sizes.find_id_offset(addr);
                rec = RWRecord(addr, (uint16_t) func_id, (uint16_t) inst_id, (uint16_t) size, is_write,
                               (uint32_t) id_offset.id, id_offset.size);
            }
            catch (std::invalid_argument&) {
                return;
            }
        } else
            rec = RWRecord(addr, (uint16_t) func_id, (uint16_t) inst_id, (uint16_t) size, is_write);
        current->log_load_store(rec);
    }
}

// Intercept the pthread_create function.
int pthread_create(pthread_t *tid, const pthread_attr_t *attr,
                   void *(*start_routine)(void *), void *arg) {
    MallocHookDeactivator deactiv;
    int res = xthread::getInstance().thread_create(tid, attr, start_routine, arg);
    return res;
}
