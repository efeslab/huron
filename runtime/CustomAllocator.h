#include <cstddef>

extern "C" {
    extern void *__libc_malloc(size_t size);
    extern void __libc_free(void *ptr);
}

template <typename T>
class Allocator {
public:
    typedef T value_type;

    Allocator() {}

    T* allocate(size_t n) { return static_cast<T*>(__libc_malloc(n * sizeof(T))); }
    
    void deallocate(T* p, size_t) { __libc_free(p); }
};
