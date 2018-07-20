#include <cstdint>
#include <cstring>
#include <unordered_map>
#include <utility>

#include <pthread.h>

#include <unordered_map>
#include <cstdint>
#include <vector>

#include <pthread.h>

#include "hooks.hpp"
#include "detector.hpp"
#include "detector/FSD.h"

const int MAX_CHARS_PER_LINE = 512;
const int MAX_TOKENS_PER_LINE = 25;
const char* const DELIMITER = " ";

extern "C" void fork_all_threads(std::vector<LaserEvent*> *hotPages);
extern "C" void fork_all_protect_all();

class detector{
  static std::unordered_map<std::uint64_t, Mem_access_type_size > PC_mem_access;
  static FSD *fsd;
  static pthread_mutex_t lock;
  static bool didOutput;

public:
  static void initialize(int window_size, int sample_period);
  static void handleRecord(DataRecord *dr);
  static void checkFalseSharing();
  static void doOutput();
};

std::unordered_map<std::uint64_t, Mem_access_type_size > detector::PC_mem_access;
FSD *detector::fsd = nullptr;
pthread_mutex_t detector::lock;
bool detector::didOutput = false;

void detector::initialize(int window_size, int sample_period)
{
  char executablePath[100];
  char mem_access_type[6];
  int mem_access_size;
  uint64_t PCTemp;
  uint64_t min_key = 0;
  uint64_t max_key = 0;

  for(int c = 0; c < 6; c++){
    mem_access_type[c] = 0;
  }

  const char *oldldpreload = getenv("LD_PRELOAD");
  setenv("LD_PRELOAD", "", 1);

  int pid = getpid();
  sprintf(executablePath,"/proc/%d/exe",pid);

  pthread_mutexattr_t mutex_attr;
  pthread_mutexattr_init(&mutex_attr);
  pthread_mutexattr_setpshared(&mutex_attr,PTHREAD_PROCESS_SHARED);

  orig_pthread_mutex_init(&lock,&mutex_attr);

  char *prefix = getenv("TMI_PREFIX");
  if (!prefix)
    _exit(1);
  string cmd = string(prefix) + "/" + "xed/xed -i ";
  cmd.append(executablePath);
  cout<<"|---> fetching op info with cmd  "<<cmd<<endl;
  string output = execArbitraryShellCmd(cmd.c_str());
  istringstream fin (output);
  
  if (!fin.good())
    cout << "Unable to open file\n";
  
  // read each line of the file
  
  while (!fin.eof()) {
    // read an entire line into memory
    char buf[MAX_CHARS_PER_LINE];
    fin.getline(buf, MAX_CHARS_PER_LINE);
    sscanf(buf,"%s Operand length in bytes = %d PC %lu",mem_access_type,&mem_access_size,&PCTemp);
    struct Mem_access_type_size mem_access_obj;
    //    std::cout << mem_access_type[0] << "\n";
    if(strncmp(mem_access_type,"load",4) == 0){
      mem_access_obj.isStore = false;
    }else if(strncmp(mem_access_type,"store",5) == 0){
      mem_access_obj.isStore = true;
    }else{
      // Skip this line if it's not introducing a new instruction
      continue;
    }
    //printf("PC: %p, Type: %s, Size: %d\n", PCTemp, mem_access_type, mem_access_size);
    mem_access_obj.size = mem_access_size;
    PC_mem_access[PCTemp] = mem_access_obj;
  }

#ifdef TMI_PROTECT
  fsd = new FSD(false);
#else
  // Disable windowing for detect-only execution
  fsd = new FSD(false);
#endif
  fsd->WindowSize = window_size;
  //fsd->SamplePeriod = sample_period;

  setenv("LD_PRELOAD", oldldpreload, 1);
}

void detector::handleRecord(DataRecord* dr)
{
  //printf("%lu %p %p\n", dr->time, dr->ip, dr->data);

  DataAccess da;
  da.PC = (uint64_t)dr->ip;
  da.Address = (uint64_t)dr->data;

  if(PC_mem_access.count(da.PC) > 0){
    struct Mem_access_type_size& mem_access_obj = PC_mem_access[da.PC];
    da.Size = mem_access_obj.size;
    if(mem_access_obj.isStore){
      da.AccessType = 'W';
    }else{
      da.AccessType = 'R';
    }
  }else{
    return;
  }

  pthread_mutex_lock(&lock);
  fsd->HandleEvent(da);
  pthread_mutex_unlock(&lock);
}

extern "C" int is_protectable(void *address);

void detector::checkFalseSharing()
{
#ifdef TMI_PROTECT
  pthread_mutex_lock(&lock);

  if(fsd->ShouldLaunchProtection()){
    if(fsd->DegradeToSheriff()){
      fprintf(stderr,"Launching TMI Protection on all pages\n");
      fork_all_protect_all();
    }else{
      std::vector<LaserEvent*> *hotPages = new std::vector<LaserEvent*>;
      if(fsd->RetrieveHotPages(hotPages)){
	bool launchProtection = false;
	for(LaserEvent *event : *hotPages){
	  if(is_protectable((void*)event->Addr)){
	    launchProtection = true;
	  }
	}
	
	if(launchProtection){
	  fprintf(stderr,"Launching TMI Protection\n");
	  fork_all_threads(hotPages);
	}
      }
    }
  }

  pthread_mutex_unlock(&lock);
#endif
}

void detector::doOutput(){
#ifndef TMI_PROTECT
  pthread_mutex_lock(&lock);
  if(didOutput){
    pthread_mutex_unlock(&lock);
    return;
  }

  char *exename = getenv("TMI_EXE_NAME");
  char *path = getenv("TMI_OUTPUT_PATH");

  if(exename == nullptr){
    exename = "default";
  }

  if(path == nullptr){
    path = "/scratch";
  }

  fsd->DumpDetectionResult(exename,path);
  didOutput = true;
  pthread_mutex_unlock(&lock);
#endif
}

extern "C"{
void detector_init(int window_size, int sample_period){
  detector::initialize(window_size, sample_period);
}

void detector_handle_record(DataRecord *dr){
  detector::handleRecord(dr);
}

void detector_check_false_sharing(){
  detector::checkFalseSharing();
}

void detector_do_output(){
  detector::doOutput();
}
}
