#include <pthread.h>
#include <dlfcn.h>

#include <string.h>

#include "tmi.h"
#include "tmimem.hpp"
#include "hooks.hpp"

extern "C"{

void tmi_asm_start(){
  tmi_commit();
}

void tmi_asm_end(){
  tmi_commit();
}

}

#ifndef TMI_NO_SYNC
int pthread_mutex_init(pthread_mutex_t *mutex, 
		       const pthread_mutexattr_t *attr){
  init_orig_functions();

  pthread_mutex_t *real_mutex = tmi_get_shared<pthread_mutex_t>(mutex);
  MUST(orig_pthread_mutex_init != nullptr);

  pthread_mutexattr_t newattr;
  pthread_mutexattr_init(&newattr);
  if(attr != nullptr){
    newattr = *attr;
  }

  pthread_mutexattr_setpshared(&newattr,PTHREAD_PROCESS_SHARED);

  return orig_pthread_mutex_init(real_mutex,&newattr);
}

int pthread_mutex_destroy(pthread_mutex_t *mutex){
  pthread_mutex_t *real_mutex = tmi_get_shared<pthread_mutex_t>(mutex);
  MUST(orig_pthread_mutex_destroy != nullptr);
  return orig_pthread_mutex_destroy(real_mutex);
}

int pthread_mutex_lock(pthread_mutex_t *mutex){
  pthread_mutex_t *real_mutex = tmi_get_shared<pthread_mutex_t>(mutex);
  tmi_commit();
  MUST(orig_pthread_mutex_lock != nullptr);
  return orig_pthread_mutex_lock(real_mutex);
}

int pthread_mutex_trylock(pthread_mutex_t *mutex){
  pthread_mutex_t *real_mutex = tmi_get_shared<pthread_mutex_t>(mutex);
  tmi_commit();
  MUST(orig_pthread_mutex_trylock != nullptr);
  return orig_pthread_mutex_trylock(real_mutex);
}

int pthread_mutex_unlock(pthread_mutex_t *mutex){
  pthread_mutex_t *real_mutex = tmi_get_shared<pthread_mutex_t>(mutex);
  tmi_commit();
  MUST(orig_pthread_mutex_unlock != nullptr);
  return orig_pthread_mutex_unlock(real_mutex);
}

int pthread_spin_init(pthread_spinlock_t *lock, int pshared){
  init_orig_functions();

  pthread_spinlock_t *real_lock = tmi_get_shared<pthread_spinlock_t>(lock);
  MUST(orig_pthread_spin_init != nullptr);

  return orig_pthread_spin_init(real_lock,PTHREAD_PROCESS_SHARED);
}

int pthread_spin_destroy(pthread_spinlock_t *lock){
  pthread_spinlock_t *real_lock = tmi_get_shared<pthread_spinlock_t>(lock);
  MUST(orig_pthread_spin_destroy != nullptr);
  return orig_pthread_spin_destroy(real_lock);
}

int pthread_spin_lock(pthread_spinlock_t *lock){
  pthread_spinlock_t *real_lock = tmi_get_shared<pthread_spinlock_t>(lock);
  tmi_commit();
  MUST(orig_pthread_spin_lock != nullptr);
  return orig_pthread_spin_lock(real_lock);
}

int pthread_spin_trylock(pthread_spinlock_t *lock){
  pthread_spinlock_t *real_lock = tmi_get_shared<pthread_spinlock_t>(lock);
  tmi_commit();
  MUST(orig_pthread_spin_trylock != nullptr);
  return orig_pthread_spin_trylock(real_lock);
}

int pthread_spin_unlock(pthread_spinlock_t *lock){
  pthread_spinlock_t *real_lock = tmi_get_shared<pthread_spinlock_t>(lock);
  tmi_commit();
  MUST(orig_pthread_spin_unlock != nullptr);
  return orig_pthread_spin_unlock(real_lock);
}

int pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *attr){
  init_orig_functions();

  pthread_cond_t *real_cond = tmi_get_shared<pthread_cond_t>(cond);
  MUST(orig_pthread_cond_init != nullptr);
  MUST(real_cond != nullptr);

  pthread_condattr_t *newattr = nullptr;
  if(attr != nullptr){
    newattr = (pthread_condattr_t*)attr;
  }else{
    newattr = new pthread_condattr_t;
    pthread_condattr_init(newattr);
  }
  MUST(pthread_condattr_setpshared(newattr,PTHREAD_PROCESS_SHARED) == 0);

  int retval = orig_pthread_cond_init(real_cond,attr);

  if(retval != 0){
    fprintf(stderr,"Error: %s\n",strerror(retval));
  }

  if(attr == nullptr){
    delete newattr;
  }

  return 0;
}

int pthread_cond_destroy(pthread_cond_t *cond){
  pthread_cond_t *real_cond = tmi_get_shared<pthread_cond_t>(cond);
  MUST(orig_pthread_cond_destroy != nullptr);
  return orig_pthread_cond_destroy(real_cond);
}

int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex){
  pthread_cond_t *real_cond = tmi_get_shared<pthread_cond_t>(cond);
  pthread_mutex_t *real_mutex = tmi_get_shared<pthread_mutex_t>(mutex);
  tmi_commit();
  MUST(orig_pthread_cond_wait != nullptr);
  return orig_pthread_cond_wait(real_cond,real_mutex);
}

int pthread_cond_signal(pthread_cond_t *cond){
  pthread_cond_t *real_cond = tmi_get_shared<pthread_cond_t>(cond);
  MUST(orig_pthread_cond_signal != nullptr);
  return orig_pthread_cond_signal(real_cond);
}

int pthread_cond_broadcast(pthread_cond_t *cond){
  pthread_cond_t *real_cond = tmi_get_shared<pthread_cond_t>(cond);
  MUST(orig_pthread_cond_broadcast != nullptr);
  return orig_pthread_cond_broadcast(real_cond);
}

int pthread_barrier_init(pthread_barrier_t *barrier,
			 const pthread_barrierattr_t *attr,
			 unsigned int count){
  init_orig_functions();
  
  pthread_barrier_t *real_barrier = tmi_get_shared<pthread_barrier_t>(barrier);
  MUST(orig_pthread_barrier_init != nullptr);

  pthread_barrierattr_t newattr = {0};  
  if(attr != nullptr){
    newattr = *attr;
  }
  pthread_barrierattr_setpshared(&newattr,PTHREAD_PROCESS_SHARED);
 
  int retval = orig_pthread_barrier_init(real_barrier,&newattr,count);

  if(retval != 0){
    perror("Barrier: ");
  }

  return retval;
}

int pthread_barrier_destroy(pthread_barrier_t *barrier){
  pthread_barrier_t *real_barrier = tmi_get_shared<pthread_barrier_t>(barrier);
  MUST(orig_pthread_barrier_destroy != nullptr);
  return orig_pthread_barrier_destroy(real_barrier);
}

int pthread_barrier_wait(pthread_barrier_t *barrier){
  pthread_barrier_t *real_barrier = tmi_get_shared<pthread_barrier_t>(barrier);
  tmi_commit();
  MUST(orig_pthread_barrier_wait != nullptr);
  return orig_pthread_barrier_wait(real_barrier);
}
#endif
