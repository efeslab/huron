#include <pthread.h>
#include <iostream>
#include <unistd.h>

#include <sys/mman.h>

#define THREADS 4
#define ITERS 100

int *count;

typedef struct condandlock_t{
  pthread_mutex_t *lock;
  pthread_cond_t *cond;
  bool is_waiting;
} condandlock;

void* do_locks(void *arg){
  pthread_mutex_t *lock = (pthread_mutex_t*)arg;

  for(int c = 0; c < ITERS; c++){
    pthread_mutex_lock(lock);
    *count = *count + 1;
    pthread_mutex_unlock(lock);
  }

  return nullptr;
}

void* do_waits(void *arg){
  condandlock *both = (condandlock*)arg;

  for(int c = 0; c < ITERS; c++){
    pthread_mutex_lock(both->lock);
    if(both->is_waiting){
      both->is_waiting = false;
      pthread_cond_signal(both->cond);
    }else{
      both->is_waiting = true;
      pthread_cond_wait(both->cond,both->lock);
    }
    pthread_mutex_unlock(both->lock);
  }

  return nullptr;
}

void* do_barriers(void *arg){
  pthread_barrier_t *barrier = (pthread_barrier_t*)arg;

  for(int c = 0; c < ITERS; c++){
    pthread_barrier_wait(barrier);
  }

  return nullptr;
}

int main(){
  pthread_barrier_t process_barrier;
  pthread_barrierattr_t attr;
  pthread_barrierattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
  pthread_barrier_init(&process_barrier,&attr,THREADS);

  void *region = mmap(0,4096,PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, 0, 0);
  count = (int*)region;
  *count = 0;
  
  pthread_t *pthreads = new pthread_t[THREADS];
  pthread_mutex_t lock1 = PTHREAD_MUTEX_INITIALIZER;
  pthread_mutex_t *lock2 = new pthread_mutex_t;
  pthread_mutex_init(lock2,NULL);

  pthread_mutex_lock(lock2);
  pthread_mutex_unlock(lock2);

  std::cout << "Starting locks with threads\n";

  for(int c = 0; c < THREADS; c++){
    pthread_create(&pthreads[c],NULL,do_locks,(void*)&lock1);
  }

  for(int c = 0; c < THREADS; c++){
    pthread_join(pthreads[c],NULL);
  }

  std::cout << "Count: " << *count << "\n";
  *count = 0;

  delete [] pthreads;

  std::cout << "Starting waits with threads\n";

  pthreads = new pthread_t[2];
  condandlock both;
  both.lock = new pthread_mutex_t;
  both.cond = new pthread_cond_t;
  pthread_mutex_init(both.lock,NULL);
  pthread_cond_init(both.cond,NULL);
  both.is_waiting = false;

  for(int c = 0; c < 2; c++){
    pthread_create(&pthreads[c],NULL,do_waits,(void*)&both);
  }

  for(int c = 0; c < 2; c++){
    pthread_join(pthreads[c],NULL);
  }

  delete [] pthreads;

  std::cout << "Starting barriers with threads\n";

  pthreads = new pthread_t[THREADS];
  pthread_barrier_t barrier;

  pthread_barrier_init(&barrier,NULL,THREADS);

  for(int c = 0; c < THREADS; c++){
    pthread_create(&pthreads[c],NULL,do_barriers,(void*)&barrier);
  }

  for(int c = 0; c < THREADS; c++){
    pthread_join(pthreads[c],NULL);
  }

  /*

  std::cout << "Forking threads\n";

  for(int c = 0; c < THREADS-1; c++){
    int pid = fork();

    if(pid == 0){
      break;
    }
  }

  delete [] pthreads;

  std::cout << "Starting wait on " << getpid() << "\n";

  pthread_barrier_wait(&process_barrier);

  std::cout << "Starting locks with processes\n";
  do_locks((void*)lock2);

  pthread_barrier_wait(&process_barrier);

  std::cout << "Count: " << *count << "\n";

  do_waits((void*)&both);

  pthread_barrier_wait(&process_barrier);
  do_barriers((void*)&barrier);
  */

  return 0;
}
