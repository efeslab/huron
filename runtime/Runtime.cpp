#include <cstdio>
#include <vector>
#include <mutex>
#include <unordered_map>
#include <csignal>

#include "MemArith.h"
#include "xthread.h"
#include "GetGlobal.h"
#include "Segment.h"

extern "C" {

uintptr_t globalStart, globalEnd;
AddrSeg heap(((uintptr_t) 1 << 63), 0);

void initializer(void) __attribute__((constructor));
void finalizer(void) __attribute__((destructor));

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
std::mutex globals_lock;

class MallocHookDeactivator {
    Thread *current_copy;
public:
    MallocHookDeactivator() noexcept: current_copy(current) { current_copy->all_hooks_active = false; }

    ~MallocHookDeactivator() noexcept { current_copy->all_hooks_active = true; }

    Thread *get_current() {
        return current_copy;
    }
};

void initializer(void) {
#ifdef DEBUG
    printf("Initializing...\n");
#endif
    getGlobalRegion(&globalStart, &globalEnd);
    printf("%p, %p\n", (void *) globalStart, (void *) globalEnd);
    xthread::getInstance().initInitialThread();
    current->all_hooks_active = true;
}

void finalizer(void) {
    current->all_hooks_active = false;
#ifdef DEBUG
    printf("Finalizing...\n");
#endif
    malloc_sizes.dump();
    xthread::getInstance().flush_all();
    // xthread::getInstance().flush_all_concat_to("record.log");
}

void *my_malloc_hook(size_t size) {
    void *start_ptr = __libc_malloc(size);
    // size = round_up_size(size, cacheline_size_power);
    // void *start_ptr = aligned_alloc(1 << cacheline_size_power, size);
    // Only record thread 0.
    // if (deactiv.get_current()->index == 0) {
    {
        // RAII deactivate malloc hook so that we can use malloc below.
        MallocHookDeactivator deactiv;
        std::lock_guard<std::mutex> lock_guard(globals_lock);
        auto start = (uintptr_t) start_ptr;
        AddrSeg seg(start, start + size);
        // Global, single-threaded
        heap.insert(seg);
        malloc_sizes.insert(start, size);
    }
    return start_ptr;
}

void *my_realloc_hook(void *ptr, size_t size) {
    void *new_start_ptr = __libc_realloc(ptr, size);
    // Only record thread 0.
    // if (deactiv.get_current()->index == 0) {
    {
        // RAII deactivate malloc hook so that we can use realloc below.
        MallocHookDeactivator deactiv;
        std::lock_guard<std::mutex> lock_guard(globals_lock);
        if (ptr) {
#ifdef DEBUG
            printf("realloc(%p, %lu)\n", ptr, size);
#endif
            auto old_start = (uintptr_t) ptr;
            size_t old_size = malloc_sizes.get_size(old_start);
            assert(old_size != MallocInfo::nfound);
            // Global, single-threaded
            heap.shrink(AddrSeg(old_start, old_size));
        }
        auto new_start = (uintptr_t) new_start_ptr;
        // Global, single-threaded
        heap.insert(AddrSeg(new_start, size));
        malloc_sizes.insert(new_start, size);
    }
    return new_start_ptr;
}

int my_posix_memalign_hook(void **memptr, size_t alignment, size_t size) {
    int code = __internal_posix_memalign(memptr, alignment, size);
    if (code)
        return code;
#ifdef DEBUG
    // printf("posix_memalign(%p, %lu, %lu)\n", *memptr, alignment, size);
#endif
    // Only record thread 0.
    // if (deactiv.get_current()->index == 0) {
    {
        // RAII deactivate malloc hook so that we can use malloc below.
        MallocHookDeactivator deactiv;
        std::lock_guard<std::mutex> lock_guard(globals_lock);
        auto start = (uintptr_t) *memptr;
        AddrSeg seg(start, start + size);
        // Global, single-threaded
        heap.insert(seg);
        malloc_sizes.insert(start, size);
    }
    return code;
}

void *malloc(size_t size) noexcept {
    // void *caller = __builtin_return_address(0);
    if (current && current->all_hooks_active)
        return my_malloc_hook(size);
    return __libc_malloc(size);
}

void *calloc(size_t n, size_t size) {
    size_t total = n * size;
    void *p = malloc(total);
    return p ? memset(p, 0, total) : nullptr;
}

void *realloc(void *ptr, size_t size) {
    if (current && current->all_hooks_active)
        return my_realloc_hook(ptr, size);
    return __libc_realloc(ptr, size);
}

int posix_memalign(void **memptr, size_t alignment, size_t size) {
    if (current && current->all_hooks_active)
        return my_posix_memalign_hook(memptr, alignment, size);
    return __internal_posix_memalign(memptr, alignment, size);
}

void my_free_hook(void *ptr) {
    // RAII deactivate malloc hook so that we can use free below.
    MallocHookDeactivator deactiv;
    free(ptr);
}

void free(void *ptr) {
    // if (current && current->all_hooks_active)
    // return my_free_hook(ptr, caller);
    return __libc_free(ptr);
}

inline void handle_access(uintptr_t addr, uint64_t func_id, uint64_t inst_id,
                          size_t size, bool is_write) {
    // Quickly return if even not in the range.
    if (!heap.contain(addr) && (addr < globalStart || addr >= globalEnd))
        return;
    MallocHookDeactivator deactiv;
    LocRecord rec = LocRecord(addr, (uint16_t) func_id, (uint16_t) inst_id, (uint16_t) size);
    deactiv.get_current()->log_load_store(rec, is_write);
}

// Intercept the pthread_create function.
int pthread_create(pthread_t *tid, const pthread_attr_t *attr,
                   void *(*start_routine)(void *), void *arg) {
    if (current && current->all_hooks_active) {
        MallocHookDeactivator deactiv;
        int res = xthread::getInstance().thread_create(tid, attr, start_routine, arg);
        return res;
    }
    else
        return __internal_pthread_create(tid, attr, start_routine, arg);
}
