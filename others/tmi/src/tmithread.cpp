#include "tmithread.hpp"
#include "tmiutil.hpp"
#include "hooks.hpp"

#include <cstdlib>
#include <sys/wait.h>

#include <stdio.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <sys/mman.h>

#include <cassert>

#ifdef TMI_PROTECT
extern "C" void* tmi_internal_alloc(size_t);
#else
#define tmi_internal_alloc malloc
#endif

tmi_thread::tmi_thread(void *(*start_routine)(void*), void *arg) :
  _tid(-1), _pid(-1), _is_process(false), _start_routine(start_routine),
  _arg(arg), _tramp(nullptr), _wait_sem(nullptr), _perf(nullptr), _isRunning(true)
{
  tmiutil::init_mutex(&_wait_forever_lock);
  tmiutil::init_cond(&_wait_forever_cond);

  tmiutil::init_mutex(&_wait_lock);
  tmiutil::init_cond(&_wait_cond);
}

void tmi_thread::set_tid(int tid)
{
  _tid = tid;
  _wait_sem = (sem_t*)tmi_internal_alloc(sizeof(sem_t));
  sem_init(_wait_sem,1,0);
}

int tmi_thread::get_tid()
{
  return _tid;
}

bool tmi_thread::init_perf(int sample_period)
{
#ifndef TMI_WITHOUT_DETECTOR
  _perf = new Perf(sample_period);
  _sample_period = sample_period;
  _perf->set_pid(_tid);
  if(_perf == nullptr || !_perf->open()){
    fprintf(stderr, "Failed opening perf counter: %p\n",_perf);
    return false;
  }
#endif
  return true;
}

Perf* tmi_thread::get_perf()
{
  return _perf;
}

void tmi_thread::cleanup_perf_thread()
{
  if(_perf != nullptr){
    _perf->disable();
    _perf->finalize();
  }
}

bool tmi_thread::is_process()
{
  return _is_process;
}

void tmi_thread::set_as_process(int new_tid)
{
  _pid = new_tid;
  _is_process = true;
}

void tmi_thread::waitOriginalThread()
{
  orig_pthread_cond_wait(&_wait_forever_cond,&_wait_forever_lock);
}

void tmi_thread::doExit(int errcode)
{
  _isRunning = false;
  sem_post(_wait_sem);
  if(_is_process){
    exit(errcode);
  }
}

bool tmi_thread::waitOn()
{
  fprintf(stderr,"Waiting on thread\n");

  while(_wait_sem == 0){
  }

  fprintf(stderr,"Doing sem_wait\n");

  if(sem_wait(_wait_sem) != 0){
    perror("sem_wait");
  }

  fprintf(stderr,"Waiting on process or done joining %d, %d\n",_pid, _tid);

  if(_is_process){
    int status;
    waitpid(_pid,&status,__WALL);
    
    return !(WIFEXITED(status) || WIFSIGNALED(status));
  }else{
    // Make main thread stop waiting
    return false;
  }
}
