//
// Created by yifanz on 6/14/18.
//

#ifndef RUNTIME_LIBFUNCS_H
#define RUNTIME_LIBFUNCS_H

#include <cstdio>
#include <dlfcn.h>
#include <zconf.h>
#include <cstdlib>
#include <cassert>

int __internal_pthread_create(pthread_t *t1, const pthread_attr_t *t2,
                              void *(*t3)(void *), void *t4) {
    typedef int (*p_create_t)(pthread_t *, const pthread_attr_t *, void *(*)(void *), void *);
    static p_create_t _pthread_create_ptr;
    if (_pthread_create_ptr == nullptr) {
        void *lib_handle = dlopen("libpthread.so.0", RTLD_NOW | RTLD_GLOBAL | RTLD_NOLOAD);
        if (lib_handle == nullptr) {
            fprintf(stderr, "Unable to load libpthread.so.0\n");
            exit(2);
        }
        _pthread_create_ptr = (p_create_t) dlsym(lib_handle, "pthread_create");
        assert(_pthread_create_ptr);
    }
    return _pthread_create_ptr(t1, t2, t3, t4);
}

int __internal_posix_memalign(void **memptr, size_t alignment, size_t size) {
    typedef int (*posix_memalign_t)(void **, size_t, size_t);
    static posix_memalign_t _posix_memalign_ptr;
    if (_posix_memalign_ptr == nullptr) {
        _posix_memalign_ptr = (posix_memalign_t) dlsym(RTLD_NEXT, "posix_memalign");
        assert(_posix_memalign_ptr);
    }
    return _posix_memalign_ptr(memptr, alignment, size);
}

extern "C" {
void *__libc_malloc(size_t size);
void __libc_free(void *ptr);
void *__libc_realloc(void *ptr, size_t size);
}

#endif //RUNTIME_LIBFUNCS_H
