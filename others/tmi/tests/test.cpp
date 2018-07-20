#include <iostream>
#include <pthread.h>
#include <cassert>

#include <sys/types.h>
#include <unistd.h>

#define N 2000000000
#define I 10

class data{
public:
  long x;
  long y;
  long z;
};

void* worker1(void *arg){
  data *d = (data*)arg;
  for(int i = 0; i < I; i++){
    for(int c = 0; c < N; c++){
      d->x++;
    }
  }
  std::cout << "Iter1: " << d->x << "\n";
  return 0;
}

void* worker2(void *arg){
  data *d = (data*)arg;
  for(int i = 0; i < I; i++){
    for(int c = 0; c < N; c++){
      d->y++;
    }
  }
  std::cout << "Iter2: " << d->y << "\n";
  return 0;
}

int main(){
  data *d = new data;
  d->x = 0;
  d->y = 0;

  pthread_t t1, t2;

  pthread_create(&t1,0,worker1,(void*)d);
  pthread_create(&t2,0,worker2,(void*)d);

  pthread_join(t1,0);
  pthread_join(t2,0);
  
  d->z = 5;

  std::cout << d->x << "," << d->y << "\n";

  return 0;
}
