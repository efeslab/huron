#include <vector>
#include <iostream>
#include <atomic>
#include <cassert>
#include <pthread.h>
#include <array>

// BOOST_SP_DISABLE_THREADS forces boost to use non-atomic refcounts, causing an assert failure at the end of the program.
//#define BOOST_SP_USE_PTHREADS
//#define BOOST_SP_DISABLE_THREADS
//#include <boost/shared_ptr.hpp>

//#define NUM_THREADS 8
const unsigned NUM_THREADS = 8;
// with 1<<28 operations total, takes about 5s to run the version with FS
const unsigned REFCOUNT_BUMPS = 1 << 8;
const unsigned FS_WRITES = 1 << 20;


typedef struct {
  int i;
#ifdef FIX_FS
  char pad[60];
#endif
} PaddedInt;

pthread_mutex_t ref_count_mutex;
unsigned long long ref_count;

typedef struct {
  int  *sp;
  //char pad0[1 << 23]; // 8MB of padding
  PaddedInt ints[NUM_THREADS] __attribute__ ((aligned (64)));
  //char pad1[1 << 23]; // 8MB of padding
  std::array< std::array<int *, REFCOUNT_BUMPS>, NUM_THREADS > sptrs;
} Globals;

Globals *G;

pthread_barrier_t b;

void *workerThread(void *tidptr) {
  unsigned tid = *(unsigned *)tidptr;

  pthread_barrier_wait(&b);

  for (unsigned i = 0; i < REFCOUNT_BUMPS; i++) {
#ifdef LOCKED
    pthread_mutex_lock(&ref_count_mutex);
#endif
    G->sptrs[tid][i] = G->sp; // increments G->sp's refcount via relaxed atomic
    ref_count+=1;
#ifdef LOCKED
    pthread_mutex_unlock(&ref_count_mutex);
#endif
    for (unsigned j = 0; j < FS_WRITES; j++) {
      G->ints[tid].i++;
    }
  }

  return nullptr;
}

int main() {
  G = (Globals *) malloc(sizeof(Globals));
  pthread_mutex_init(&ref_count_mutex, NULL);
  ref_count = 0;

  pthread_barrier_init(&b,NULL,8);

  // validate memory layout by printing the cache line each element resides in
  std::cout << "ints[0]:" << std::hex << (((long)&G->ints[0]) >> 6) << 
    " ints[1]:" << (((long)&G->ints[1]) >> 6) << 
    " ints[7]:" << (((long)&G->ints[7]) >> 6) << std::endl;

  G->sp=new int(42);
  //assert(1 == G->sp.use_count());

  std::vector<pthread_t> threads;
  std::vector<unsigned> args;
  for (unsigned i = 0; i < NUM_THREADS; i++) {
    threads.emplace_back();
    args.push_back(i);
  }

  for (size_t i = 0; i < NUM_THREADS; i++) 
    pthread_create(&threads[i], nullptr, workerThread, &args[i]);

  for (auto t : threads)
    pthread_join(t, nullptr);

  // assert that sp's refcount has been tracked appropriately
  //assert(1 + (REFCOUNT_BUMPS * NUM_THREADS) == G->sp.use_count());
  
  return 0;

}
