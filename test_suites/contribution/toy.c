#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>

#define THREAD_COUNT 4
#define TOTAL 1024
int ITER = 100000;

int *dynMemory[10];

void *run1(void *ptr)
{
  int start = *((int *)ptr);
  for(int i=start; i < TOTAL; i+=THREAD_COUNT)
  {
    for(int j = 0; j < ITER; j++)
    {
      int val = dynMemory[0][i];
      if(j==0)val=0;
      else{
        if(j%2)val+=1;
        else val+=2;
      }
      dynMemory[0][i]=val;
    }
  }
  pthread_exit(NULL);
}

void *run2(void *ptr)
{
  int start = *((int *)ptr);
  for(int i=start; i < TOTAL; i+=THREAD_COUNT)
  {
    for(int j = 0; j < ITER; j++)
    {
      int val = dynMemory[1][i];
      if(j==0)val=0;
      else{
        if(j%2)val+=1;
        else val+=2;
      }
      dynMemory[1][i]=val;
    }
  }
  pthread_exit(NULL);
}

void *run3(void *ptr)
{
  int start = *((int *)ptr);
  for(int i=start; i < TOTAL; i+=THREAD_COUNT)
  {
    for(int j = 0; j < ITER; j++)
    {
      int val = dynMemory[2][i];
      if(j==0)val=0;
      else{
        if(j%2)val+=1;
        else val+=2;
      }
      dynMemory[2][i]=val;
    }
  }
  pthread_exit(NULL);
}

void *run4(void *ptr)
{
  int start = *((int *)ptr);
  for(int i=start; i < TOTAL; i+=THREAD_COUNT)
  {
    for(int j = 0; j < ITER; j++)
    {
      int val = dynMemory[3][i];
      if(j==0)val=0;
      else{
        if(j%2)val+=1;
        else val+=2;
      }
      dynMemory[3][i]=val;
    }
  }
  pthread_exit(NULL);
}

void *run5(void *ptr)
{
  int start = *((int *)ptr);
  for(int i=start; i < TOTAL; i+=THREAD_COUNT)
  {
    for(int j = 0; j < ITER; j++)
    {
      int val = dynMemory[4][i];
      if(j==0)val=0;
      else{
        if(j%2)val+=1;
        else val+=2;
      }
      dynMemory[4][i]=val;
    }
  }
  pthread_exit(NULL);
}

void *run6(void *ptr)
{
  int start = *((int *)ptr);
  for(int i=start; i < TOTAL; i+=THREAD_COUNT)
  {
    for(int j = 0; j < ITER; j++)
    {
      int val = dynMemory[5][i];
      if(j==0)val=0;
      else{
        if(j%2)val+=1;
        else val+=2;
      }
      dynMemory[5][i]=val;
    }
  }
  pthread_exit(NULL);
}

void *run7(void *ptr)
{
  int start = *((int *)ptr);
  for(int i=start; i < TOTAL; i+=THREAD_COUNT)
  {
    for(int j = 0; j < ITER; j++)
    {
      int val = dynMemory[6][i];
      if(j==0)val=0;
      else{
        if(j%2)val+=1;
        else val+=2;
      }
      dynMemory[6][i]=val;
    }
  }
  pthread_exit(NULL);
}

void *run8(void *ptr)
{
  int start = *((int *)ptr);
  for(int i=start; i < TOTAL; i+=THREAD_COUNT)
  {
    for(int j = 0; j < ITER; j++)
    {
      int val = dynMemory[7][i];
      if(j==0)val=0;
      else{
        if(j%2)val+=1;
        else val+=2;
      }
      dynMemory[7][i]=val;
    }
  }
  pthread_exit(NULL);
}

void *run9(void *ptr)
{
  int start = *((int *)ptr);
  for(int i=start; i < TOTAL; i+=THREAD_COUNT)
  {
    for(int j = 0; j < ITER; j++)
    {
      int val = dynMemory[8][i];
      if(j==0)val=0;
      else{
        if(j%2)val+=1;
        else val+=2;
      }
      dynMemory[8][i]=val;
    }
  }
  pthread_exit(NULL);
}

void *run10(void *ptr)
{
  int start = *((int *)ptr);
  for(int i=start; i < TOTAL; i+=THREAD_COUNT)
  {
    for(int j = 0; j < ITER; j++)
    {
      int val = dynMemory[9][i];
      if(j==0)val=0;
      else{
        if(j%2)val+=1;
        else val+=2;
      }
      dynMemory[9][i]=val;
    }
  }
  pthread_exit(NULL);
}

int main(int argc, char *argv[])
{
  if(argc > 1)ITER=atoi(argv[1]);
  dynMemory[0] = (int * )malloc(TOTAL*sizeof(int));
  pthread_t threads[THREAD_COUNT];
  int params[THREAD_COUNT];
  for(int i = 0; i < THREAD_COUNT; i++)
  {
    params[i]=i;
  }
  for(int i = 0; i < THREAD_COUNT; i++)
  {
    pthread_create(&threads[i], NULL, run1, ((void *)(&params[i])));
  }
  for(int i = 0; i < THREAD_COUNT; i++)
  {
    pthread_join(threads[i], NULL);
  }
  if(argc > 2 && atoi(argv[2]) < 2)return 0;
  dynMemory[1] = (int *)malloc(TOTAL*sizeof(int));
  for(int i = 0; i < THREAD_COUNT; i++)
  {
    pthread_create(&threads[i], NULL, run2, ((void *)(&params[i])));
  }
  for(int i = 0; i < THREAD_COUNT; i++)
  {
    pthread_join(threads[i], NULL);
  }
  if(argc > 2 && atoi(argv[2]) < 3)return 0;
  dynMemory[2] = (int *)malloc(TOTAL*sizeof(int));
  for(int i = 0; i < THREAD_COUNT; i++)
  {
    pthread_create(&threads[i], NULL, run3, ((void *)(&params[i])));
  }
  for(int i = 0; i < THREAD_COUNT; i++)
  {
    pthread_join(threads[i], NULL);
  }
  if(argc > 2 && atoi(argv[2]) < 4)return 0;
  dynMemory[3] = (int *)malloc(TOTAL*sizeof(int));
  for(int i = 0; i < THREAD_COUNT; i++)
  {
    pthread_create(&threads[i], NULL, run4, ((void *)(&params[i])));
  }
  for(int i = 0; i < THREAD_COUNT; i++)
  {
    pthread_join(threads[i], NULL);
  }
  if(argc > 2 && atoi(argv[2]) < 5)return 0;
  dynMemory[4] = (int *)malloc(TOTAL*sizeof(int));
  for(int i = 0; i < THREAD_COUNT; i++)
  {
    pthread_create(&threads[i], NULL, run5, ((void *)(&params[i])));
  }
  for(int i = 0; i < THREAD_COUNT; i++)
  {
    pthread_join(threads[i], NULL);
  }
  if(argc > 2 && atoi(argv[2]) < 6)return 0;
  dynMemory[5] = (int *)malloc(TOTAL*sizeof(int));
  for(int i = 0; i < THREAD_COUNT; i++)
  {
    pthread_create(&threads[i], NULL, run6, ((void *)(&params[i])));
  }
  for(int i = 0; i < THREAD_COUNT; i++)
  {
    pthread_join(threads[i], NULL);
  }
  if(argc > 2 && atoi(argv[2]) < 7)return 0;
  dynMemory[6] = (int *)malloc(TOTAL*sizeof(int));
  for(int i = 0; i < THREAD_COUNT; i++)
  {
    pthread_create(&threads[i], NULL, run7, ((void *)(&params[i])));
  }
  for(int i = 0; i < THREAD_COUNT; i++)
  {
    pthread_join(threads[i], NULL);
  }
  if(argc > 2 && atoi(argv[2]) < 8)return 0;
  dynMemory[7] = (int *)malloc(TOTAL*sizeof(int));
  for(int i = 0; i < THREAD_COUNT; i++)
  {
    pthread_create(&threads[i], NULL, run8, ((void *)(&params[i])));
  }
  for(int i = 0; i < THREAD_COUNT; i++)
  {
    pthread_join(threads[i], NULL);
  }
  if(argc > 2 && atoi(argv[2]) < 9)return 0;
  dynMemory[8] = (int *)malloc(TOTAL*sizeof(int));
  for(int i = 0; i < THREAD_COUNT; i++)
  {
    pthread_create(&threads[i], NULL, run9, ((void *)(&params[i])));
  }
  for(int i = 0; i < THREAD_COUNT; i++)
  {
    pthread_join(threads[i], NULL);
  }
  if(argc > 2 && atoi(argv[2]) < 10)return 0;
  dynMemory[9] = (int *)malloc(TOTAL*sizeof(int));
  for(int i = 0; i < THREAD_COUNT; i++)
  {
    pthread_create(&threads[i], NULL, run10, ((void *)(&params[i])));
  }
  for(int i = 0; i < THREAD_COUNT; i++)
  {
    pthread_join(threads[i], NULL);
  }
  return 0;
}
