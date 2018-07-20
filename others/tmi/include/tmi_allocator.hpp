#pragma once

#include <cstdlib>
#include <memory>

extern "C" void* tmi_internal_alloc(size_t size);
extern "C" void tmi_internal_dealloc(void *p);

template<typename T> class tmi_allocator : public std::allocator<T>{
public:
  inline T* allocate(size_t cnt, const T* hint = 0)
  {
    return (T*)tmi_internal_alloc(cnt * sizeof(T));
  }

  inline void deallocate( T* p, size_t n )
  {
    tmi_internal_dealloc((void*)p);
  }
};
