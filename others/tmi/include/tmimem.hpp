#pragma once

#include "pagestate.hpp"
#include "tmi_allocator.hpp"

#include <cstdlib>
#include <pthread.h>
#include <unordered_map>
#include <utility>

#include <atomic>

typedef std::unordered_map<size_t,pagestate*,
			   std::hash<size_t>,
			   std::equal_to<size_t>,
			   tmi_allocator<std::pair<const size_t, pagestate*>>> PageStateMap;

extern "C" void* get_shared(void *base,size_t size,int type);
extern "C" void tmi_commit();

extern "C" void* tmi_internal_alloc(size_t size);
extern "C" void tmi_internal_dealloc(void *p);

template <typename T> inline T* tmi_get_shared(T* base){
  return (T*)get_shared((void*)base,sizeof(T),0);
}

template <>
inline pthread_mutex_t* tmi_get_shared(pthread_mutex_t* base){
  return (pthread_mutex_t*)get_shared((void*)base,sizeof(pthread_mutex_t),1);
}

template <>
inline pthread_cond_t* tmi_get_shared(pthread_cond_t* base){
  return (pthread_cond_t*)get_shared((void*)base,sizeof(pthread_cond_t),2);
}

template <>
inline pthread_spinlock_t* tmi_get_shared(pthread_spinlock_t* base){
  return (pthread_spinlock_t*)get_shared((void*)base,sizeof(pthread_spinlock_t),3);
}

class memory{
  static const size_t DEFAULT_TWIN_STATE_SIZE = 4096;

  static char *_persistentMemory;
  static char *_slabStart;
  static char *_slabEnd;
  static char *_slabPos;
  static char *_mmapPos;
  static char *_mmapPos2;
  
  static int _backingFd;

  static bool _initialized;

  static pagestate *_states;

public:
  static bool init();
  static void cleanup();

  // Memory protection mechanisms for preventing false-sharing
  static int isProtectable(void *addr);

  static pagestate* getState(void *addr);
  static void allocStates(size_t size);
  static bool protectAddress(void *addr);
  static void protectAll();
  static void handleSegv(int sig, void *addr);
  static void commit(int pid);

  // Functions that provide memory to the memory allocator
  static void* adjust_slab_memory(size_t size);
  static void* get_mmap_memory(size_t size);

  static void* get_shared(void *base, size_t size, int type = 0);
};
