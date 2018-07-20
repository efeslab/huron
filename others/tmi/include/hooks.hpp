#pragma once

#include <cstdlib>
#include <pthread.h>

extern "C" int (*orig_pthread_create)(pthread_t *thread, 
				      const pthread_attr_t *attr, 
				      void *(*start_routine)(void*), 
				      void *arg);
extern "C" int (*orig_pthread_join)(pthread_t thread, void **retval);
extern "C" void (*orig_pthread_exit)(void *arg);

extern "C" void* (*orig_malloc)(size_t size);
extern "C" void (*orig_free)(void *p);

extern "C" int (*orig_pthread_mutex_init)(pthread_mutex_t *mutex,const pthread_mutexattr_t *attr);
extern "C" int (*orig_pthread_mutex_destroy)(pthread_mutex_t *mutex);
extern "C" int (*orig_pthread_mutex_lock)(pthread_mutex_t *mutex);
extern "C" int (*orig_pthread_mutex_trylock)(pthread_mutex_t *mutex);
extern "C" int (*orig_pthread_mutex_unlock)(pthread_mutex_t *mutex);

extern "C" int (*orig_pthread_spin_init)(pthread_spinlock_t *lock, int pshared);
extern "C" int (*orig_pthread_spin_destroy)(pthread_spinlock_t *lock);
extern "C" int (*orig_pthread_spin_lock)(pthread_spinlock_t *mutex);
extern "C" int (*orig_pthread_spin_trylock)(pthread_spinlock_t *mutex);
extern "C" int (*orig_pthread_spin_unlock)(pthread_spinlock_t *mutex);

extern "C" int (*orig_pthread_cond_init)(pthread_cond_t *cond, const pthread_condattr_t *attr);
extern "C" int (*orig_pthread_cond_destroy)(pthread_cond_t *cond);
extern "C" int (*orig_pthread_cond_wait)(pthread_cond_t *cond, pthread_mutex_t *mutex);
extern "C" int (*orig_pthread_cond_signal)(pthread_cond_t *cond);
extern "C" int (*orig_pthread_cond_broadcast)(pthread_cond_t *cond);

extern "C" int (*orig_pthread_barrier_init)(pthread_barrier_t *barrier,
					    const pthread_barrierattr_t *attr,
					    unsigned int count);
extern "C" int (*orig_pthread_barrier_destroy)(pthread_barrier_t *barrier);
extern "C" int (*orig_pthread_barrier_wait)(pthread_barrier_t *barrier);

extern "C" void init_orig_functions();
