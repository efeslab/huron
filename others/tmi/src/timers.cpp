#include "timers.hpp"

#include <iostream>
#include <cstdio>
#include <sys/mman.h>

unsigned long *timers::_segvTime = 0;
unsigned long *timers::_commitTime = 0;

clock_type::time_point timers::_startSegv;
clock_type::time_point timers::_startCommit;

void timers::initTimer(int name)
{
  void *locs = mmap(0,4096,PROT_READ | PROT_WRITE,
		    MAP_ANONYMOUS | MAP_SHARED,0,0);

  _segvTime = (unsigned long*)locs;
  _commitTime = _segvTime + 1;
}

void timers::startTimer(int name)
{
#ifdef TMI_TIMERS
  if(name == 1){
    _startSegv = clock_type::now();
  }else if(name == 2){
    _startCommit = clock_type::now();
  }
#endif
}

void timers::stopTimer(int name)
{
#ifdef TMI_TIMERS
  clock_type::time_point end = clock_type::now();

  auto t = end - _startSegv;

  if(name == 1){
    *_segvTime += (end - _startSegv).count();
  }else if (name == 2){
    *_commitTime += (end - _startCommit).count();
  }
#endif
}

void timers::printTimers()
{
#ifdef TMI_TIMERS
  fprintf(stderr,"SEGV: %lu\n",*_segvTime);
  fprintf(stderr,"COMMIT: %lu\n",*_commitTime);
#endif
}
