#include <cstdio>
#include <vector>
#include <mutex>
#include <unordered_map>
#include <pthread.h>

#include "MemArith.h"
#include "xthread.h"
#include "GetGlobal.h"

extern "C" {

void *globalStart, *globalEnd;
uintptr_t heapStart = ((uintptr_t) 1 << 63), heapEnd;

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

MallocInfo malloc_sizes;
std::mutex other_globals_lock;

class MallocHookDeactivator {
    Thread *current_copy;
public:
    MallocHookDeactivator() noexcept: current_copy(current) { current_copy->malloc_hook_active = false; }

    ~MallocHookDeactivator() noexcept { current_copy->malloc_hook_active = true; }

    Thread *get_current() {
        return current_copy;
    }
};

void initializer(void) {
#ifdef DEBUG
    printf("Initializing...\n");
#endif
    xthread::getInstance().initialize();
    getGlobalRegion(&globalStart, &globalEnd);
    current->malloc_hook_active = true;
}

void finalizer(void) {
    // RAII deactivate malloc hook so that we can use printf (and malloc) below.
    MallocHookDeactivator deactiv;
#ifdef DEBUG
    printf("Finalizing...\n");
#endif
    malloc_sizes.dump();
    xthread::getInstance().flush_all_concat_to("record.log");
}

void *my_malloc_hook(size_t size, const void *caller) {
    // RAII deactivate malloc hook so that we can use malloc below.
    MallocHookDeactivator deactiv;
    // void *start_ptr = malloc(size);
    size = round_up_size(size, cacheline_size_power);
    void *start_ptr = aligned_alloc(1 << cacheline_size_power, size);
    uintptr_t start = (uintptr_t)start_ptr, end = start + size;
    // Only record thread 0.
    if (deactiv.get_current()->index != 0)
        return start_ptr;
    // The 3 lines below are single threaded.
    heapStart = std::min(heapStart, start);
    heapEnd = std::max(heapEnd, end);
    malloc_sizes.insert(start, size);
#ifdef DEBUG
    // printf("malloc(%lu) returns %p\n", size, start_ptr);
    // printf("heapStart = %p, heapEnd = %p\n", heapStart, heapEnd);
#endif
    return start_ptr;
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

inline void handle_access(uintptr_t addr, uint64_t func_id, uint64_t inst_id,
                   size_t size, bool is_write) {
    // Quickly return if even not in the range.
    if (addr < heapStart || addr >= heapEnd)
        return;
    MallocHookDeactivator deactiv;
    LocRecord rec = LocRecord(addr, (uint16_t) func_id, (uint16_t) inst_id, (uint16_t) size);
    deactiv.get_current()->log_load_store(rec, is_write);
}

// Intercept the pthread_create function.
int pthread_create(pthread_t *tid, const pthread_attr_t *attr,
                   void *(*start_routine)(void *), void *arg) {
    MallocHookDeactivator deactiv;
    int res = xthread::getInstance().thread_create(tid, attr, start_routine, arg);
    return res;
}
