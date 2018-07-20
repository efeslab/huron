#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

#define TMISO_NAME "/home/delozier/tmi/build/lib/libtmialloc.so"

char *period = nullptr;
char *windowSize = nullptr;
char *outputPath = nullptr;

static void addArg(const char *name, char *value)
{
  if(value != nullptr){
    setenv(name,value,1);
  }
}

static int run_child(int argc, char* argv[])
{ 
  setenv("LD_PRELOAD", TMISO_NAME, 1);

  for(int c = 1; c < argc; c++){
    argv[c-1] = argv[c];
  }
  argv[argc-1] = nullptr;

  execvp(argv[0], argv);
  printf("execvp failed!\n");
  return -1;
}

int main(int argc, char* argv[])
{
  if(argc < 2) {
    printf("Syntax error: %s <prog> [args...]\n",argv[0]);
    return -1;
  }
  
  return run_child(argc,argv);
}

