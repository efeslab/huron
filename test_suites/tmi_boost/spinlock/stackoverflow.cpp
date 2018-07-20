//#include <boost/shared_ptr.hpp>
//#include <boost/timer.hpp>
#include <iostream>
#include <vector>
#include <stdlib.h>
#include <thread>
#include <cstring>
#include "spinlock.hpp"
using namespace std;

enum { BufferSize = 1<<16,  SLsPerCacheLine = 1 };

int          ibuffer[BufferSize];

//using boost::detail::spinlock;
size_t nslp = 41;
spinlock* pslp = 0;

spinlock& getSpinlock(size_t h)
{
  return pslp[ (h%nslp) * SLsPerCacheLine ];
}


void threadFunc(int offset)
{
  const size_t mask = BufferSize-1;
  for (size_t ii=0, index=(offset&mask); ii<BufferSize; ++ii, index=((index+1)&mask))
    {
      spinlock& sl = getSpinlock(index);
      sl.lock();
      ibuffer[index] += 1;
      sl.unlock();
    }
};


int main(int argc, char* argv[])
{
  cout << "Using pool size: "<< nslp << endl;
  cout << "sizeof(spinlock): "<< sizeof(spinlock) << endl;
  cout << "SLsPerCacheLine: "<< int(SLsPerCacheLine) << endl;
  const size_t num = nslp * SLsPerCacheLine;
  pslp = (spinlock *) malloc(sizeof(spinlock)*num);
  /*for (size_t ii=0; ii<num ; ii++)
    { memset(pslp+ii,0,sizeof(*pslp)); }*/

  const size_t nThreads = 4;

  std::thread* ppThreads[nThreads];
  const int offset[nThreads] = { 1, 2, 4, 5 };

  //boost::timer timer;

  for (size_t ii=0; ii<nThreads; ii++)
    { ppThreads[ii] = new std::thread(threadFunc, offset[ii]); }

  for (size_t ii=0; ii<nThreads; ii++)
    { ppThreads[ii]->join(); }

  //cout << "Elapsed time: " << timer.elapsed() << endl;

  for (size_t ii=0; ii<nThreads; ii++)
    { delete ppThreads[ii]; }

  free(pslp);

  return 0;
}
