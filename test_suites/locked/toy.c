#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>

#define THREAD_COUNT 4
#define TOTAL 1024
int ITER = 1000000;

int *dynMemory;
pthread_mutex_t ownThreadLock[THREAD_COUNT*64];

void *run(void *ptr)
{
  int start = *((int *)ptr);
  printf("%d\n",start);
  for(int i = start; i < TOTAL; i+= 4*THREAD_COUNT)
  {
    for(int j = 0; j < ITER; j++)
    {
      pthread_mutex_lock(&ownThreadLock[start*64]);
      int val=dynMemory[i];
      if(j == 0)val=0;
      else {
        if(j%2)val+=1;
        else val+=2;
      }
      dynMemory[i]=val;
      val=dynMemory[i+THREAD_COUNT];
      if(j == 0)val=0;
      else {
        if(j%2)val+=1;
        else val+=2;
      }
      dynMemory[i+THREAD_COUNT]=val;
      val=dynMemory[i+2*THREAD_COUNT];
      if(j == 0)val=0;
      else {
        if(j%2)val+=1;
        else val+=2;
      }
      dynMemory[i+2*THREAD_COUNT]=val;
      val=dynMemory[i+3*THREAD_COUNT];
      if(j == 0)val=0;
      else {
        if(j%2)val+=1;
        else val+=2;
      }
      dynMemory[i+3*THREAD_COUNT]=val;
      pthread_mutex_unlock(&ownThreadLock[start*64]);
    }
  }
  pthread_exit(NULL);
}

int main(int argc, char *argv[])
{
  if(argc > 1)ITER=atoi(argv[1]);
  dynMemory = (int *)malloc(TOTAL*sizeof(int));
  pthread_t threads[THREAD_COUNT];
  int params[THREAD_COUNT];
  for (int i = 0; i < THREAD_COUNT; i++)
  {
    params[i]=i;
    pthread_mutex_init(&ownThreadLock[i*64], NULL);
  }
  for (int i = 0; i < THREAD_COUNT; i++)
  {
    pthread_create(&threads[i], NULL, run, ((void *)(&params[i])));
  }
  for (int i = 0; i < THREAD_COUNT; i++)
  {
    pthread_join(threads[i], NULL);
  }
  return 0;
}

