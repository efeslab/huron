#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <sys/mman.h>

int main(){
  char fname[100];
  sprintf(fname,"/tmp/tmi-backing-XXXXXX");
  int backingFd = mkstemp(fname);
  if(backingFd == -1){
    perror("Failed to make backing file\n");
    return 1;
  }

  int pagesize = getpagesize();

  if(ftruncate(backingFd,pagesize)){
    perror("ftruncate: ");
    return 1;
  }

  void *p = malloc(pagesize);

  int *data = (int*)p;
  data[0] = 5;

  void* ret = mmap((void*)data,sizeof(int) * 1024, PROT_READ | PROT_WRITE,
		     MAP_SHARED | MAP_FIXED,backingFd,0);

  if(ret == MAP_FAILED){
    perror("MMAP: ");
    return 1;
  }

  printf("%p - %p\n",(void*)data,ret);

  if(fork() == 0){
    data[0] = 20;
    printf("Changed data[0]\n");
  }else{
    sleep(5);
    printf("%d\n",data[0]);
  }

  munmap(data,pagesize);

  close(backingFd);

  return 0;
}
