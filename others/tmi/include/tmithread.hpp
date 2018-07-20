#pragma once

#include "perf.hpp"

#include <atomic>
#include <cstdio>
#include <pthread.h>

#include <semaphore.h>

class tmi_thread{
  int _tid; // Set using gettid() or getpid()
  int _pid;
  bool _is_process;

public:
  pthread_t _pthreadid;
  void *(*_start_routine)(void*);
  void *_arg;
  void *_tramp;

  pthread_mutex_t _wait_forever_lock;
  pthread_cond_t _wait_forever_cond;

  pthread_mutex_t _wait_lock;
  pthread_cond_t _wait_cond;

  sem_t * _wait_sem;

  void *_ret;

private:
  Perf *_perf;
  int _sample_period;

  std::atomic<bool> _isRunning;

public:
  tmi_thread(void *(start_routine)(void*), void *arg);

  void set_tid(int tid);
  int get_tid();

  bool is_process();

  bool init_perf(int sample_period);
  Perf* get_perf();
  void cleanup_perf_thread();

  void set_as_process(int new_tid);

  void waitOriginalThread();
  void doExit(int errcode = 0);
  bool waitOn();
};
