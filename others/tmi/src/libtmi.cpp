#define _GNU_SOURCE

#include "detector/laser_api.h"

#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <ucontext.h>

#include <sys/wait.h>

#include <vector>
#include <map>

#include <pthread.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <sys/mman.h>

#include "perf.hpp"
#include "hooks.hpp"
#include "detector.hpp"
#include "tmi.h"
#include "tmimem.hpp"
#include "tmithread.hpp"
#include "timers.hpp"
#include "internalmem.hpp"
#include "tramps.h"
#include "funchook.h"

#pragma GCC diagnostic ignored "-Wunused-function" // XXX

static __thread INJECT_TRAMPOLINE* THREAD_INJECT_TRAMPOLINE;

static pthread_t *detector_tid;
static bool keep_going = true;
static tmi_shared_data* gTSD;
static pthread_mutex_t sch_lock;

static std::vector<LaserEvent*> *_hotPages = nullptr;
static bool _protectAll = false;

#define TMI_MAX_THREADS 1024

std::atomic<int> threadsInitSync;
std::atomic<int> threadsInSync;
std::atomic<bool> stopSync;

tmi_thread **all_contexts = nullptr;
std::atomic<int> nextTid;
thread_local int myTid;

static int SAMPLE_PERIOD = SAMPLE_PERIOD_DEFAULT;

extern "C" void init_orig_functions();
extern "C" void protect_address(void *address);
extern "C" void protect_all();

extern "C" {

#ifdef TMI_PROTECT

static void init_shmem()
{
  char* shmem_name = getenv(TMI_SHMEM_NAME);
  int fd;
  MUST(shmem_name != NULL);
  
  fd = shm_open(shmem_name, O_RDWR, S_IRUSR | S_IWUSR);
  MUST(fd >= 0);
  gTSD = (tmi_shared_data*)mmap(NULL, TMI_SHMEM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  MUST(gTSD != MAP_FAILED);

  pthread_mutexattr_t mutex_attr;
  pthread_mutexattr_init(&mutex_attr);
  pthread_mutexattr_setpshared(&mutex_attr,PTHREAD_PROCESS_SHARED);

  orig_pthread_mutex_init(&sch_lock,&mutex_attr);

  close(fd);
}

#endif

int main_pid = 0;

void tmi_exit(int errcode = 0)
{
  if(getpid() == main_pid){
    keep_going = false;
#ifndef TMI_PROTECT
    detector_do_output();
#endif
#ifdef TMI_PROTECT
    sch_write(gTSD->child_pipe_fd,SCH_EVT_EXIT,getpid(),getpid(),NULL);
    timers::printTimers();
#endif
    memory::cleanup();
  }

  _exit(errcode);
}

void tmi_start()
{
  main_pid = getpid();
  fprintf(stderr,"Doing tmi_init() in %d\n",main_pid);
  atexit((void (*)(void))tmi_exit);
  init_orig_functions();

#ifdef TMI_TIMERS
  timers::initTimer(0);
#endif

#ifdef TMI_PROTECT 
  init_shmem();
  if(!memory::init()){
    fprintf(stderr,"Memory failed to initialize!\n");
    exit(-1);
  }
#endif
}

void die(const char* msg, ...)
{
    va_list ap;
    va_start(ap, msg);
    vfprintf(stderr, msg, ap);
    exit(-1);
}

////// BACKGROUND THREADS ///////

void parse_perf_events(tmi_thread *thread)
{
  if(thread != nullptr){
    Perf *perf = thread->get_perf();
   
    if(perf != nullptr){
      perf->disable();
      
      if(!perf->start_iterate()){
	printf("Failed iterating performance counter!\n");
	return;
      }

      DataRecord dr;
      while (perf->next(&dr)) {
	detector_handle_record(&dr);
      }
      
      perf->enable();
    }
  }
}

void* detector_thread(void *arg)
{
  while(keep_going){
    sleep(1);

    int totalTids = nextTid;
    
    for(int tid = 0; tid < totalTids; tid++){
      tmi_thread *thread = all_contexts[tid];
      if(thread != nullptr){
	parse_perf_events(thread);
      }
    }

    detector_check_false_sharing();
  }

  int totalTids = nextTid;
  for(int tid = 0; tid < totalTids; tid++){
    tmi_thread *thread = all_contexts[tid];
    if(thread != nullptr){
      thread->cleanup_perf_thread();
    }
  }
}

static tmi_thread* get_my_context()
{
  return all_contexts[myTid];
}

static tmi_thread* get_context(pthread_t tid)
{
  tmi_thread *ctx = nullptr;
  int totalTids = nextTid;
  
  for(int c = 0; c < totalTids; c++){
    tmi_thread *context = all_contexts[c];
    if(context != nullptr && pthread_equal(context->_pthreadid,tid)){
      ctx = context;
    }
  }
  return ctx;
}

static void tramp_function()
{
  printf("in tramp_function\n");
  pthread_t tid = pthread_self();
  tmi_thread *context = get_my_context();

  MUST(context != nullptr);

  if(_protectAll){
    protect_all();
    _protectAll = false;
  }else{
    if(_hotPages != nullptr){
      for(LaserEvent *event : *_hotPages){
	protect_address((void*)event->Addr);
      }
      _hotPages = nullptr;
    }
  }

  printf("Forking\n");

  pid_t pid = fork();

  stopSync = false;

  if(pid == -1){
    fprintf(stderr,"Failed to fork!\n");
    exit(1);
  }

  if(pid != 0) {
    context->set_as_process(pid);
    pthread_exit(NULL);
  }else{
    MUST(gTSD != nullptr);
    sch_write(gTSD->child_pipe_fd, SCH_EVT_RESUME, getpid(), getpid(), NULL);
    
    while(1){}
  }
}

pid_t gettid( void ) 
{ 
    return syscall( __NR_gettid ); 
} 

void sch_write(int fd, SCH_EVT_TYPE type, pid_t mpid, pid_t pid, void* data)
{
  orig_pthread_mutex_lock(&sch_lock);
  int old_errno;
  sch_evt_t evt;
  evt.type = type;
  evt.mpid = mpid;
  evt.pid = pid;
  evt.data = data;
  old_errno = errno;
  MUST(write(fd, &evt, sizeof(evt)) == sizeof(evt));
  errno = old_errno;
  orig_pthread_mutex_unlock(&sch_lock);
}

void fork_all_protect_all()
{
  keep_going = false; // Kill off detector thread
  static bool already_forked = false;
  if(!already_forked){
    // Wait for threads in synchronization to exit it
    stopSync = true;
    while(threadsInitSync > 0 || threadsInSync > 0) {}

    _protectAll = true;
    already_forked = true;
    sch_write(gTSD->child_pipe_fd, SCH_EVT_FORCE_FORK, 0, 0, 0);
  }
}

void fork_all_threads(std::vector<LaserEvent*> *hotPages)
{
  keep_going = false; // Kill off detector thread
  static bool already_forked = false;
  if(!already_forked){
    // Wait for threads in synchronization to exit it
    stopSync = true;
    while(threadsInitSync > 0 || threadsInSync > 0) {}

    _hotPages = hotPages;
    already_forked = true;
    sch_write(gTSD->child_pipe_fd, SCH_EVT_FORCE_FORK, 0, 0, 0);
  }
}

static void register_new_thread(tmi_thread* context)
{
  MUST(context != nullptr);
  context->_pthreadid = pthread_self();
  myTid = nextTid++;
  all_contexts[myTid] = context;
  context->set_tid(gettid());
#ifdef TMI_PROTECT
  context->_tramp = (void*)tramp_function;//THREAD_INJECT_TRAMPOLINE;
  sch_write(gTSD->child_pipe_fd, SCH_EVT_NEW_THREAD, getpid(), gettid(), &context->_tramp);
#endif
}

static void* pthread_start_routine(void* arg)
{
  int tid = gettid();
  tmi_thread* context = (tmi_thread*)arg;
  //fprintf(stderr,"*** RUNNING IN NEW THREAD: %i\n", tid);

  register_new_thread(context);

  if(myTid < 16){
    context->init_perf(SAMPLE_PERIOD);
  }

  // Run the actual thread routine
  void *ret = context->_start_routine(context->_arg);

  context->_ret = ret;

  // This code ends up getting called either if the thread was not 
  // turned into a process or after the new process finishes its task
  // Thus, we need to handle both cases here

  MUST(context != nullptr);
#ifdef TMI_PROTECT
  if(context->is_process()){
    tmi_commit();
  }
#endif

  fprintf(stderr,"Thread exiting %i\n",tid);

  context->doExit(0);

  return ret;
}

extern "C" void *
gomp_thread_start (void *xdata){
  fprintf(stderr,"Caught openmp thread\n");
  return nullptr;
}

////// HOOKS ///////
int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                          void *(*start_routine) (void *), void *arg)
{
  static bool isInitialized = false;

  if(!isInitialized){
    tmi_start();

#ifndef TMI_WITHOUT_DETECTOR
    int window_size = WINDOW_SIZE_DEFAULT;
    char *sample = getenv("TMI_SAMPLE_PERIOD");
    if(sample != nullptr){
      SAMPLE_PERIOD = atoi(sample);
      fprintf(stderr,"TMI_SAMPLE_PERIOD: %d\n",SAMPLE_PERIOD);
    }else{
      fprintf(stderr,"Using default TMI_SAMPLE_PERIOD\n");
    }

    char *window_size_str = getenv("TMI_WINDOW_SIZE");
    if(window_size_str != nullptr){
      window_size = atoi(window_size_str);
      fprintf(stderr,"TMI_WINDOW_SIZE: %d\n",window_size);
    }else{
      fprintf(stderr,"Using default TMI_WINDOW_SIZE\n");
    }

    detector_init(window_size,SAMPLE_PERIOD);
    detector_tid = new pthread_t;

    MUST(detector_tid != nullptr);
    MUST(orig_pthread_create != nullptr);
    orig_pthread_create(detector_tid,NULL,detector_thread,NULL);
#endif

    all_contexts = (tmi_thread**)tmi_internal_alloc(sizeof(tmi_thread*) * TMI_MAX_THREADS);

    isInitialized = true;
    fprintf(stderr,"Done with init\n");
  }

#ifdef TMI_PROTECT
  void *contextplace = (void*)tmi_internal_alloc(sizeof(tmi_thread));
  MUST(contextplace != nullptr);
  tmi_thread *context = new (contextplace) tmi_thread(start_routine,arg);
  MUST(context != nullptr);
#else
  tmi_thread *context = new tmi_thread(start_routine,arg);
  MUST(context != nullptr);
#endif

  fprintf(stderr,"Hooked new thread\n");

  return orig_pthread_create(thread, attr, pthread_start_routine, context);
}

#ifdef TMI_PROTECT
void pthread_exit(void *arg)
{
  pthread_t thread = pthread_self();
  tmi_thread *context = get_my_context();
  MUST(context != nullptr);

  context->_ret = arg;

  if(context->is_process()){
    // Will be terminated when main thread exits
    context->waitOriginalThread();
  }else{
    context->doExit(0);
    orig_pthread_exit(arg);
  }
}

int pthread_join(pthread_t thread, void **retval)
{
  tmi_thread *context = get_context(thread);

  while(context == nullptr){
    context = get_context(thread);
  }

  MUST(context != nullptr);

  while(context->waitOn()){
  }

  if(retval != NULL){
    *retval = context->_ret;
  }

  sch_write(gTSD->child_pipe_fd, SCH_EVT_THREAD_EXIT, context->get_tid(), context->get_tid(), NULL);

  return 0;
}
#endif

}
 
#ifdef TMI_PROTECT 
// XXX hack hack hack
void exit(int code)
{
  tmi_exit();
}
#endif
