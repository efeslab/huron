#include <cstdio>
#include <vector>
#include <mutex>
#include <unordered_map>
#include <csignal>

#include "MemArith.h"
#include "xthread.h"
#include "GetGlobal.h"
#include "MallocInfo.h"

extern "C" {
void initializer(void) __attribute__((constructor));
void finalizer(void) __attribute__((destructor));

void handle_access(uintptr_t addr, uint64_t func_id, uint64_t inst_id,
                   size_t size, bool is_write);

void *malloc_inst(size_t size, uint64_t func_id, uint64_t inst_id);

void *calloc_inst(size_t n, size_t size, uint64_t func_id, uint64_t inst_id);

void *realloc_inst(void *ptr, size_t size, uint64_t func_id, uint64_t inst_id);

int posix_memalign_inst(void **memptr, size_t alignment, size_t size, 
                        uint64_t func_id, uint64_t inst_id);

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
AddrSeg global;
std::atomic<size_t> thread0_alloc(0), total_alloc(0);

void initializer(void) {
#ifdef DEBUG
    printf("Initializing...\n");
#endif
    global = getGlobalRegion();
    xthread::getInstance().initInitialThread();
    current->all_hooks_active = true;
}

void finalizer(void) {
    current->all_hooks_active = false;
#ifdef DEBUG
    printf("Finalizing...\n");
    printf("Thread 0 alloc'ed %lu bytes (accumulative) out of total %lu;\n",
           thread0_alloc.load(), total_alloc.load());
#endif
    xthread::getInstance().flush_all_concat_to("record.log");
    malloc_sizes.dump("mallocRuntimeIDs.txt");
}

void *malloc_inst(size_t size, uint64_t func_id, uint64_t inst_id) {
    void *start_ptr = __libc_malloc(size);
    // size = round_up_size(size, cacheline_size_power);
    // void *start_ptr = aligned_alloc(1 << cacheline_size_power, size);
    // RAII deactivate malloc hook so that we can use malloc below.
    HookDeactivator deactiv;
    total_alloc += size;
    // Only record thread 0.
    if (deactiv.get_current()->index == 0) {
        // Global, single-threaded
        thread0_alloc += size;
        malloc_sizes.insert((uintptr_t) start_ptr, size, func_id, inst_id);
    }
    return start_ptr;
}

void *calloc_inst(size_t n, size_t size, uint64_t func_id, uint64_t inst_id) {
    size_t total = n * size;
    void *p = malloc_inst(total, func_id, inst_id);
    return p ? memset(p, 0, total) : nullptr;
}

void *realloc_inst(void *ptr, size_t size, uint64_t func_id, uint64_t inst_id) {
    void *new_start_ptr = __libc_realloc(ptr, size);
    // RAII deactivate malloc hook so that we can use realloc below.
    HookDeactivator deactiv;
    total_alloc += size;
    // Only record thread 0.
    if (deactiv.get_current()->index == 0) {
        thread0_alloc += size;
        if (ptr) {
            // Global, single-threaded
            assert(malloc_sizes.erase((uintptr_t) ptr));
        }
        // Global, single-threaded
        malloc_sizes.insert((uintptr_t) new_start_ptr, size, func_id, inst_id);
    }
    return new_start_ptr;
}

int posix_memalign_inst(void **memptr, size_t alignment, size_t size, 
                        uint64_t func_id, uint64_t inst_id) {
    int code = __internal_posix_memalign(memptr, alignment, size);
    if (code)
        return code;
    total_alloc += size;
    // RAII deactivate malloc hook so that we can use malloc below.
    HookDeactivator deactiv;
    // Only record thread 0.
    if (deactiv.get_current()->index == 0) {
        thread0_alloc += size;
        // Global, single-threaded
        malloc_sizes.insert((uintptr_t) *memptr, size, func_id, inst_id);
    }
    return code;
}

void *malloc(size_t size) noexcept {
    if (current && current->all_hooks_active) {
        HookDeactivator deactiv;
        fprintf(stderr, "Code is visiting uninstrumented malloc.\n");
    }
    return __libc_malloc(size);
}

void *calloc(size_t n, size_t size) {
    if (current && current->all_hooks_active) {
        HookDeactivator deactiv;
        fprintf(stderr, "Code is visiting uninstrumented calloc.\n");
    }
    size_t total = n * size;
    void *p = __libc_malloc(total);
    return p ? memset(p, 0, total) : nullptr;
}

void *realloc(void *ptr, size_t size) {
    if (current && current->all_hooks_active) {
        HookDeactivator deactiv;
        fprintf(stderr, "Code is visiting uninstrumented realloc.\n");
    }
    return __libc_realloc(ptr, size);
}

int posix_memalign(void **memptr, size_t alignment, size_t size) {
    if (current && current->all_hooks_active) {
        HookDeactivator deactiv;
        fprintf(stderr, "Code is visiting uninstrumented posix_memalign "
                        "(because we don't instrument posix_memalign at this time.\n");
    }
    return __internal_posix_memalign(memptr, alignment, size);
}

void my_free_hook(void *ptr) {
    // RAII deactivate malloc hook so that we can use free below.
    HookDeactivator deactiv;
    if (ptr && deactiv.get_current()->index == 0)
        malloc_sizes.erase((uintptr_t) ptr);
    __libc_free(ptr);
}

void free(void *ptr) {
    if (current && current->all_hooks_active) {
        my_free_hook(ptr);
        return;
    }
    __libc_free(ptr);
}

void handle_access(uintptr_t addr, uint64_t func_id, uint64_t inst_id,
                   size_t size, bool is_write) {
    // If on heap:
    if (malloc_sizes.contain(addr)) {
        HookDeactivator deactiv;
        size_t m_id, m_offset;
        bool is_recorded = malloc_sizes.find_id_offset(addr, m_id, m_offset);
        if (is_recorded) {
            LocRecord rec = LocRecord(addr, (uint32_t) func_id, (uint32_t) inst_id, (uint16_t) size,
                                      (uint32_t) m_id, (uint32_t) m_offset);
            deactiv.get_current()->log_load_store(rec, is_write);
        }
    } else if (global.contain(addr)) { // If on global:
        HookDeactivator deactiv;
        LocRecord rec = LocRecord(addr, (uint32_t) func_id, (uint32_t) inst_id, (uint16_t) size);
        deactiv.get_current()->log_load_store(rec, is_write);
    }
}

// Intercept the pthread_create function.
int pthread_create(pthread_t *tid, const pthread_attr_t *attr,
                   void *(*start_routine)(void *), void *arg) {
    if (current && current->all_hooks_active) {
        HookDeactivator deactiv;
        int res = xthread::getInstance().thread_create(tid, attr, start_routine, arg);
        return res;
    } else
        return __internal_pthread_create(tid, attr, start_routine, arg);
}
