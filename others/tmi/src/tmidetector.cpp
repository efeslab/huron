#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <string>

#define TMISO_NAME "src/.lib/libtmi.so"

char *period = nullptr;
char *windowSize = nullptr;
char *outputPath = nullptr;
char *exeName = nullptr;

static void addArg(const char *name, char *value)
{
  if(value != nullptr){
    setenv(name,value,1);
  }
}

static int run_child(int argc, char* argv[])
{ 
  char *prefix = getenv("TMI_PREFIX");
  if (!prefix)
    _exit(1);
  setenv("LD_PRELOAD", (std::string(prefix) + "/" + TMISO_NAME).c_str(), 1);

  addArg("TMI_SAMPLE_PERIOD",period);
  addArg("TMI_WINDOW_SIZE",windowSize);
  addArg("TMI_EXE_NAME",exeName);
  addArg("TMI_OUTPUT_PATH",outputPath);

  for(int c = 0; c < argc; c++){
    printf("%s\n",argv[c]);
  }

  execvp(exeName, argv);
  printf("execvp failed!\n");
  return -1;
}

int main(int argc, char* argv[])
{
  if(argc < 2) {
    printf("Syntax error: %s [detector_args...] <prog> [args...]\n",argv[0]);
    printf("Use option --tmi-help for a list of command-line options\n");
    return -1;
  }

  int position = 0;

  for(int c = 1; c < argc; c+=2){
    printf("%s\n",argv[c]);
    if(strcmp(argv[c],"--tmi-period") == 0){
      period = argv[c+1];
    }else if(strcmp(argv[c],"--tmi-window") == 0){
      windowSize = argv[c+1];
    }else if(strcmp(argv[c],"--tmi-path") == 0){
      outputPath = argv[c+1];
    }else if(strcmp(argv[c],"--tmi-help") == 0){
      printf("Options for TMI false-sharing detector:\n");
      printf("----------------------------------------\n\n");
      printf("  --tmi-period [####] - Sample period for HITM detection events\n");
      printf("  --tmi-window [####] - Window size for false-sharing detector\n");
      printf("  --tmi-path [/path/to/output] - Output path for false-sharing detection results\n");
      return -1;
    }else{
      printf("EXE: %s\n",argv[c]);
      exeName = argv[c];
      position = c;
      break;
    }
  }
  
  char **new_argv = new char*[argc-position+1];
  for(int c = position; c < argc; c++){
    new_argv[c-position] = argv[c];
  }
  new_argv[argc-position] = nullptr;
  
  return run_child(argc-position,new_argv);
}

