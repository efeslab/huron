#include "tmiutil.hpp"

#include <hooks.hpp>
#include <cstdlib>
#include <pthread.h>
#include <cassert>

#include <cstring>
#include <cstdio>

void tmiutil::init_mutex(pthread_mutex_t *mutex)
{
  pthread_mutexattr_t attr {0};
  assert(pthread_mutexattr_setpshared(&attr, 
				      PTHREAD_PROCESS_SHARED) 
	 == 0);
  assert(orig_pthread_mutex_init(mutex,&attr) == 0);
}

void tmiutil::init_cond(pthread_cond_t *cond)
{
  pthread_condattr_t attr;
  assert(pthread_condattr_init(&attr) == 0);
  assert(pthread_condattr_setpshared(&attr, 
				     PTHREAD_PROCESS_SHARED)
	 == 0);
  orig_pthread_cond_init(cond,&attr);
}

void tmiutil::init_spinlock(pthread_spinlock_t *mutex)
{
  orig_pthread_spin_init(mutex,PTHREAD_PROCESS_SHARED);
}

void* tmiutil::alignAddressToPage(void *addr)
{
#ifdef TMI_USING_HUGEPAGES
  size_t num = (size_t)addr;
  //           0x0000000000200000
  //           0x00007faf35c00000
  num = num & (0xFFFFFFFFFFC00000);
  return (void*)num;
#else
  size_t num = (size_t)addr;
  num = num & (0xFFFFFFFFFFFFF000);
  return (void*)num;
#endif
}


