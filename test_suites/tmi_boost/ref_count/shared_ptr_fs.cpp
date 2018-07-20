#include <thread>
#include <vector>
#include <iostream>
#include <atomic>
#include <cassert>


// BOOST_SP_DISABLE_THREADS forces boost to use non-atomic refcounts, causing an assert failure at the end of the program.
//#define BOOST_SP_USE_PTHREADS
//#define BOOST_SP_DISABLE_THREADS
//#include <boost/shared_ptr.hpp>

//#define NUM_THREADS 8
const unsigned NUM_THREADS = 8;
// with 1<<28 operations total, takes about 5s to run the version with FS
const unsigned REFCOUNT_BUMPS = 1 << 8;
const unsigned FS_WRITES = 1 << 17;


typedef struct {
  int i;
#ifdef FIX_FS
  char pad[60];
#endif
} PaddedInt;

typedef struct {
  int  *sp;
  //char pad0[1 << 23]; // 8MB of padding
  PaddedInt ints[NUM_THREADS] __attribute__ ((aligned (64)));
  //char pad1[1 << 23]; // 8MB of padding
  std::array< std::array<int *, REFCOUNT_BUMPS>, NUM_THREADS > sptrs;
} Globals;

Globals *G;

pthread_barrier_t b;

void workerThread(const int tid) {

  pthread_barrier_wait(&b);

  for (unsigned i = 0; i < REFCOUNT_BUMPS; i++) {

    G->sptrs[tid][i] = G->sp; // increments G->sp's refcount via relaxed atomic
    
    for (unsigned j = 0; j < FS_WRITES; j++) {
      G->ints[tid].i++;
    }
  }
}

int main() {
  G = (Globals *) malloc(sizeof(Globals));

  pthread_barrier_init(&b,NULL,8);

  // validate memory layout by printing the cache line each element resides in
  std::cout << "ints[0]:" << std::hex << (((long)&G->ints[0]) >> 6) << 
    " ints[1]:" << (((long)&G->ints[1]) >> 6) << 
    " ints[7]:" << (((long)&G->ints[7]) >> 6) << std::endl;

  G->sp=new int(42);
  //assert(1 == G->sp.use_count());

  std::vector<std::thread*> threads;
  for (unsigned i = 0; i < NUM_THREADS; i++) {
    std::thread* t = new std::thread(workerThread, i);
    threads.push_back(t);
  }

  for (auto t : threads) {
    t->join();
    delete t;
  }

  // assert that sp's refcount has been tracked appropriately
  //assert(1 + (REFCOUNT_BUMPS * NUM_THREADS) == G->sp.use_count());
  
  return 0;

}
