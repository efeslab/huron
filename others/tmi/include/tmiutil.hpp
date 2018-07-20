#pragma once

#include <pthread.h>

class tmiutil{
public:
  static void init_mutex(pthread_mutex_t *mutex);
  static void init_cond(pthread_cond_t *cond);
  static void init_spinlock(pthread_spinlock_t *lock);
  static void* alignAddressToPage(void *addr);
};
