#include <stdio.h>
#include <pthread.h>

#define THREAD_COUNT 16
#define TOTAL 1024
#define ITER 1000000

int *dynMemory;

void *run(void *ptr)
{
  int start = *((int *)ptr);
  for(int i = start; i < TOTAL; i+= THREAD_COUNT)
  {
    for(int j = 0; j < ITER; j++)
    {
      if(j == 0)dynMemory[i]=0;
      else {
        if(j%2)dynMemory[i]+=1;
        else dynMemory[i]+=2;
      }
    }
  }
  pthread_exit(NULL);
}

int main(void)
{
  dynMemory = new int[TOTAL];
  pthread_t threads[THREAD_COUNT];
  int *params = new int[THREAD_COUNT];
  for (int i = 0; i < THREAD_COUNT; i++)
  {
    params[i]=i;
    pthread_create(&threads[i], NULL, run, ((void *)(&params[i])));
  }
  for (int i = 0; i < THREAD_COUNT; i++)
  {
    pthread_join(threads[i], NULL);
  }
  return 0;
}
