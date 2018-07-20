#include "tmi.h"

#include <pthread.h>
#include <cstdlib>
#include <dlfcn.h>
#include <cstdio>

extern "C"{

int (*orig_pthread_create)(pthread_t *thread, 
				    const pthread_attr_t *attr, 
				    void *(*start_routine)(void*), 
				    void *arg);
int (*orig_pthread_join)(pthread_t thread, void **retval);
void (*orig_pthread_exit)(void *arg);

void* (*orig_malloc)(size_t size);
void (*orig_free)(void *p);

int (*orig_pthread_mutex_init)(pthread_mutex_t *mutex,const pthread_mutexattr_t *attr);
int (*orig_pthread_mutex_destroy)(pthread_mutex_t *mutex);
int (*orig_pthread_mutex_lock)(pthread_mutex_t *mutex);
int (*orig_pthread_mutex_trylock)(pthread_mutex_t *mutex);
int (*orig_pthread_mutex_unlock)(pthread_mutex_t *mutex);

int (*orig_pthread_spin_init)(pthread_spinlock_t *lock, int pshared);
int (*orig_pthread_spin_destroy)(pthread_spinlock_t *lock);
int (*orig_pthread_spin_lock)(pthread_spinlock_t *mutex);
int (*orig_pthread_spin_trylock)(pthread_spinlock_t *mutex);
int (*orig_pthread_spin_unlock)(pthread_spinlock_t *mutex);

int (*orig_pthread_cond_init)(pthread_cond_t *cond, const pthread_condattr_t *attr);
int (*orig_pthread_cond_destroy)(pthread_cond_t *cond);
int (*orig_pthread_cond_wait)(pthread_cond_t *cond, pthread_mutex_t *mutex);
int (*orig_pthread_cond_signal)(pthread_cond_t *cond);
int (*orig_pthread_cond_broadcast)(pthread_cond_t *cond);

int (*orig_pthread_barrier_init)(pthread_barrier_t *barrier, 
				 const pthread_barrierattr_t *attr,
				 unsigned int count);
int (*orig_pthread_barrier_destroy)(pthread_barrier_t *barrier);
int (*orig_pthread_barrier_wait)(pthread_barrier_t *barrier);

void init_orig_functions()
{
  static bool initialized = false;

  if(initialized){
    return;
  }

  initialized = true;

  fprintf(stderr,"Initializing original functions\n");

  MUST(NULL != (orig_pthread_create = 
		(int (*)(pthread_t*,const pthread_attr_t*,
			   void*(*)(void*), void*))dlsym(RTLD_NEXT, "pthread_create")));
  MUST(NULL != (orig_pthread_join = 
		(int (*)(pthread_t,void**))dlsym(RTLD_NEXT, "pthread_join")));
  MUST(NULL != (orig_pthread_exit = 
		(void (*)(void*))dlsym(RTLD_NEXT, "pthread_exit")));

  MUST(NULL != (orig_malloc = 
		(void* (*)(size_t))dlsym(RTLD_NEXT, "malloc")));
  MUST(NULL != (orig_free =
		(void (*)(void*))dlsym(RTLD_NEXT, "free")));

  MUST(NULL != (orig_pthread_mutex_init = 
		(int (*)(pthread_mutex_t*, const pthread_mutexattr_t*))
		dlsym(RTLD_NEXT, "pthread_mutex_init")));
  MUST(NULL != (orig_pthread_mutex_destroy = 
		(int (*)(pthread_mutex_t*))dlsym(RTLD_NEXT, "pthread_mutex_destroy")));
  MUST(NULL != (orig_pthread_mutex_lock = 
		(int (*)(pthread_mutex_t*))dlsym(RTLD_NEXT, "pthread_mutex_lock")));
  MUST(NULL != (orig_pthread_mutex_trylock = 
		(int (*)(pthread_mutex_t*))dlsym(RTLD_NEXT, "pthread_mutex_trylock")));
  MUST(NULL != (orig_pthread_mutex_unlock = 
		(int (*)(pthread_mutex_t*))dlsym(RTLD_NEXT, "pthread_mutex_unlock")));

  MUST(NULL != (orig_pthread_spin_init = 
		(int (*)(pthread_spinlock_t*, int))dlsym(RTLD_NEXT, "pthread_spin_init")));
  MUST(NULL != (orig_pthread_spin_destroy = 
		(int (*)(pthread_spinlock_t*))dlsym(RTLD_NEXT, "pthread_spin_destroy")));
  MUST(NULL != (orig_pthread_spin_lock = 
		(int (*)(pthread_spinlock_t*))dlsym(RTLD_NEXT, "pthread_spin_lock")));
  MUST(NULL != (orig_pthread_spin_trylock = 
		(int (*)(pthread_spinlock_t*))dlsym(RTLD_NEXT, "pthread_spin_trylock")));
  MUST(NULL != (orig_pthread_spin_unlock = 
		(int (*)(pthread_spinlock_t*))dlsym(RTLD_NEXT, "pthread_spin_unlock")));

  MUST(NULL != (orig_pthread_cond_init = 
		(int (*)(pthread_cond_t*, const pthread_condattr_t *))
		dlsym(RTLD_NEXT, "pthread_cond_init")));
  MUST(NULL != (orig_pthread_cond_destroy = 
		(int (*)(pthread_cond_t*))dlsym(RTLD_NEXT, "pthread_cond_destroy")));
  MUST(NULL != (orig_pthread_cond_wait = 
		(int (*)(pthread_cond_t*, pthread_mutex_t*))dlsym(RTLD_NEXT, "pthread_cond_wait")));
  MUST(NULL != (orig_pthread_cond_signal = 
		(int (*)(pthread_cond_t*))dlsym(RTLD_NEXT, "pthread_cond_signal")));
  MUST(NULL != (orig_pthread_cond_broadcast = 
		(int (*)(pthread_cond_t*))dlsym(RTLD_NEXT, "pthread_cond_broadcast")));

  MUST(NULL != (orig_pthread_barrier_init = 
		(int (*)(pthread_barrier_t*,const pthread_barrierattr_t *,unsigned int))
		dlsym(RTLD_NEXT, "pthread_barrier_init")));
  MUST(NULL != (orig_pthread_barrier_destroy = 
		(int (*)(pthread_barrier_t*))dlsym(RTLD_NEXT, "pthread_barrier_destroy")));
  MUST(NULL != (orig_pthread_barrier_wait = 
		(int (*)(pthread_barrier_t*))dlsym(RTLD_NEXT, "pthread_barrier_wait")));

  fprintf(stderr,"Done initializing original functions\n");
}

}
