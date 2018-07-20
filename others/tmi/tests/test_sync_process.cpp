#include <pthread.h>
#include <iostream>
#include <unistd.h>

#include <sys/mman.h>

#define THREADS 4
#define ITERS 100

int *count;

void* do_locks(void *arg){
  pthread_mutex_t *lock = (pthread_mutex_t*)arg;

  for(int c = 0; c < ITERS; c++){
    pthread_mutex_lock(lock);
    *count = *count + 1;
    pthread_mutex_unlock(lock);
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
  void *region = mmap(0,4096,PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, 0, 0);
  count = (int*)region;
  *count = 0;

  pthread_barrier_t process_barrier;
  pthread_barrier_init(&process_barrier,NULL,THREADS);

  pthread_barrier_t barrier;
  pthread_barrier_init(&barrier,NULL,THREADS);
  
  pthread_mutex_t *lock2 = new pthread_mutex_t;
  pthread_mutexattr_t mutexattr;
  pthread_mutexattr_setpshared(&mutexattr, PTHREAD_PROCESS_SHARED);
  pthread_mutex_init(lock2,&mutexattr);

  pthread_mutex_lock(lock2);
  pthread_mutex_unlock(lock2);

  for(int c = 0; c < THREADS-1; c++){
    int pid = fork();

    if(pid == 0){
      break;
    }
  }

  std::cout << "Starting wait on " << getpid() << "\n";

  pthread_barrier_wait(&process_barrier);

  std::cout << "Starting locks with processes\n";
  do_locks((void*)lock2);

  pthread_barrier_wait(&process_barrier);
  std::cout << "Value: " << *count << "\n";

  std::cout << "Starting barriers with processes\n";
  do_barriers((void*)&barrier);
  std::cout << "Done with barriers with processes\n";

  pthread_barrier_wait(&process_barrier);

  

  return 0;
}
