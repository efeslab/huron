#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <dlfcn.h>
#include <signal.h>
#include <string.h>
#include <asm/mman.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unordered_map>
#include <execinfo.h>

#include "timers.hpp"
#include "tmi.h"
#include "hooks.hpp"
#include "tmiutil.hpp"
#include "pagestate.hpp"
#include "tmimem.hpp"

#include "internalmem.hpp"

//#define HEAP_SIZE 1073741824 // 1 GB
#define HEAP_SIZE 4294967296 // 4 GB
//#define HEAP_SIZE 17179869184 // 16 GB
//#define HEAP_SIZE 34359738368
//#define HEAP_SIZE 28991029248

#ifdef TMI_USING_HUGEPAGES
#define TMI_PAGESIZE 2097152
#define TMI_MMAP_FLAGS MAP_HUGETLB | MAP_HUGE_2MB
#else
#define TMI_PAGESIZE 4096
#define TMI_MMAP_FLAGS 0
#endif

extern "C" void segvhandle(int sig, siginfo_t *si, void *unused);
extern "C" void tmi_exit();

#ifndef TMI_PROTECT
void mem_exit()
{
  memory::cleanup();
}

__attribute__((constructor))
void mem_start()
{
  memory::init();
  atexit((void (*)(void))mem_exit);
}
#endif

bool memory::init()
{
  if(_initialized){
    return true;
  }

  //atexit((void (*)(void))tmi_exit);

  fprintf(stderr,"Initializing memory in %d\n",getpid());
#ifdef TMI_PROTECT
  internalmemory::init();
#endif

#ifdef TMI_USING_HUGEPAGES
  char fname[100];
  sprintf(fname,"/mnt/huge/tmi-backing-XXXXXX");
  _backingFd = mkstemp(fname);
  if(_backingFd == -1){
    perror("Failed to make backing file\n");
  }
#else
#ifdef TMI_USING_MKSTEMP
  // shm_open seems to not work for some applications (facesim)
  char fname[100];
  sprintf(fname,"/dev/shm/tmi-backing-XXXXXX");
  _backingFd = mkstemp(fname);
  if(_backingFd == -1){
    perror("Failed to make backing file\n");
  }
#else
  // For applications with large memory footprints, shm_open is much faster
  // than mkstemp
  _backingFd = shm_open("/tmi", O_CREAT | O_RDWR | O_TRUNC, 0666);
  if(_backingFd == -1){
    perror("shm_open");
  }
#endif // TMI_USING_MKSTEMP
#endif // TMI_USING_HUGEPAGES
  
  // Must ftruncate to resize to the correct size, otherwise
  // will get a bus error
  if(ftruncate(_backingFd,HEAP_SIZE)){
    perror("ftruncate: ");
  }
  
  // Persistent memory is a mapping that mirrors the private pages, but
  // is always shared to allow writes back into the file
  // 
  // Writing to the private pages can be accomplished by writing to the
  // _persistentMemory and then calling madvise(...,MADV_DONTNEED) to
  // ensure that the next copy-on-write copies from the file that has
  // been updated by writing to this shared mapping
  // 
  // Basically, the file is mapped twice - once as a shared region and
  // again as a private region
  _persistentMemory = (char*)mmap(0,HEAP_SIZE,PROT_READ | PROT_WRITE,
				  MAP_SHARED,_backingFd,0);

  _slabPos = (char*)mmap(0,HEAP_SIZE,PROT_READ | PROT_WRITE,
			 MAP_SHARED | TMI_MMAP_FLAGS,_backingFd,0);

  fprintf(stderr,"TMI Heap Allocated at %p\n",(void*)_slabPos);

  _slabStart = _slabPos;
  _slabEnd = _slabStart + HEAP_SIZE;
  
  if(_slabPos == MAP_FAILED){
    perror("mmap: ");
  }
  
  // Allocate mmaps from the end of the heap, sbrk from the top
  _mmapPos = _slabPos + (HEAP_SIZE);

  _initialized = true;

#ifdef TMI_PROTECT
  struct sigaction sa;

  sa.sa_flags = SA_SIGINFO;
  sigemptyset(&sa.sa_mask);
  sa.sa_sigaction = segvhandle;
  if (sigaction(SIGSEGV, &sa, NULL) == -1){
    fprintf(stderr,"Failed to initialize segv handler\n");
  }
#endif  

  fprintf(stderr,"Finished initializing memory\n");

  return true;
}

void memory::cleanup()
{
  fprintf(stderr,"Total slab used: %lu\n",(_slabPos - _slabStart));
  fprintf(stderr,"Total mmap used: %lu\n",(_slabEnd - _mmapPos));
  //fprintf(stderr,"Total heap used: %lu\n",(_slabPos - _slabStart) + (_slabEnd - _mmapPos));
  
  munmap(_slabStart,HEAP_SIZE);
  munmap(_persistentMemory,HEAP_SIZE);

#ifdef TMI_USING_HUGEPAGES
  close(_backingFd);
#else
#ifdef TMI_USING_MKSTEMP
  close(_backingFd);
#else
  shm_unlink("tmi");
#endif
#endif

#ifdef TMI_PROTECT
  internalmemory::cleanup();
#endif
}

#ifdef TMI_PROTECT
int memory::isProtectable(void *addr)
{
  return ((size_t)addr >= (size_t)_slabStart && 
	  (size_t)addr < (size_t)_slabEnd);
}

pagestate* memory::getState(void *addr)
{
  void *pageaddr = tmiutil::alignAddressToPage(addr);
  pagestate *state = nullptr;
  int *numStates = (int*)_states;
  pagestate *real_states = (pagestate*)((int*)_states+1);
  for(int c = 0; c < *numStates; c++){
    if(real_states[c]._addr == pageaddr){
      return &real_states[c];
    }
  }

  real_states[*numStates]._addr = addr;

  real_states[*numStates]._twin = mmap(0,TMI_PAGESIZE,PROT_READ | PROT_WRITE,
				       MAP_PRIVATE | MAP_ANONYMOUS | TMI_MMAP_FLAGS,0,0);

  if(real_states[*numStates]._twin == MAP_FAILED){
    fprintf(stderr,"Failed to allocate twin state\n");
    perror("mmap");
  }

  if(real_states[*numStates]._twin == NULL){
    fprintf(stderr,"NULL TWIN!!!\n");
  }

  real_states[*numStates]._active = false;

  *numStates = *numStates + 1;

  return &real_states[*numStates-1];
}

void memory::allocStates(size_t size)
{
  if(_states != nullptr){
    return;
  }

  size_t totalStates = size / TMI_PAGESIZE;
  size_t totalSize = totalStates * sizeof(pagestate);
  size_t pages = totalSize / getpagesize();
  if(totalSize % TMI_PAGESIZE != 0){
    pages++;
  }

  fprintf(stderr,"Allocating %lu pages to cover %lu sized memory\n",pages,size);

  // This is allocated private to allow each new process to 
  // track it's twin states locally
  _states = (pagestate*)mmap(0,getpagesize()*pages,PROT_READ | PROT_WRITE,
			     MAP_ANONYMOUS | MAP_PRIVATE, 0,0);

  fprintf(stderr,"Allocated states with size: %lu\n",getpagesize() * pages);

  if(_states == MAP_FAILED){
    fprintf(stderr,"Failed to create twin page state\n");
    return;
  }
}

void memory::protectAll()
{
  allocStates((_slabPos - _slabStart) + (_slabEnd - _mmapPos));

  void *ret = mmap(_slabStart,(_slabPos - _slabStart),PROT_READ,
		   MAP_FIXED | MAP_PRIVATE | TMI_MMAP_FLAGS, _backingFd, 0);

  if(ret == MAP_FAILED){
    fprintf(stderr,"Failed to protect slab pages\n");
    perror("Mmap");
  }

  if(_mmapPos != _slabEnd){
    fprintf(stderr,"MMAP SIZE: %lu\n",_slabEnd-_mmapPos);
    char *alignedPos = (char*)tmiutil::alignAddressToPage((void*)_mmapPos);
    void *ret = mmap(alignedPos,(_slabEnd-alignedPos),PROT_READ | PROT_WRITE,
		     MAP_FIXED | MAP_PRIVATE | TMI_MMAP_FLAGS, _backingFd, (alignedPos-_slabStart));

    if(ret == MAP_FAILED){
      fprintf(stderr,"Failed to protect mmap pages\n");
      perror("Mmap");
    }
  }
}

bool memory::protectAddress(void *addr)
{
  allocStates(TMI_PAGESIZE);

  if((size_t)addr < (size_t)_slabStart || 
     (size_t)addr > (size_t)_slabEnd){
    fprintf(stderr,"Address %p not within protected region\n",addr);
    return false;
  }

  addr = tmiutil::alignAddressToPage(addr);

  size_t addrs = (size_t)addr;


  if(addrs % TMI_PAGESIZE != 0){
    fprintf(stderr,"Address not aligned to page size!\n");
  }

  void *ret = mmap(addr,getpagesize(),PROT_READ,
		   MAP_FIXED | MAP_PRIVATE | TMI_MMAP_FLAGS, _backingFd, ((char*)addr)-_slabStart);

  if(ret == MAP_FAILED){
    fprintf(stderr,"Failed to protect page\n");
    return false;
  }

  return true;
}

void memory::handleSegv(int sig, void *addr)
{
  timers::startTimer(timers::SEGV);

  int pid = getpid();
  //fprintf(stderr,"Handling segv for pid %d at addr %p\n",pid,addr);

  // Compute the page that holds this address.
  unsigned long * pageStart = (unsigned long *) tmiutil::alignAddressToPage(addr);

  //fprintf(stderr,"Handled segv from %p to %p\n",pageStart,((char*)pageStart)+4096);

  pagestate *state = getState(addr);
  MUST(state != nullptr);
  state->_active = true;

  // Unprotect the page and record the write.
  mprotect ((char *) pageStart, TMI_PAGESIZE, PROT_READ | PROT_WRITE);

  //fprintf(stderr,"Did protect %d\n",pid);

  // Forces copy on write
#if defined(X86_32BIT)
  asm volatile ("movl %0, %1 \n\t"
		:   // Output, no output
		: "r"(pageStart[0]),  // Input
		  "m"(pageStart[0])
		: "memory");
#else
  asm volatile ("movq %0, %1 \n\t"
		:   // Output, no output
		: "r"(pageStart[0]),  // Input
		  "m"(pageStart[0])
		: "memory");
#endif

  
  MUST(state->_twin != nullptr);
  MUST(pageStart != nullptr);
  // Create the "origTwinPage" from the transient page.
  memcpy(state->_twin, pageStart, TMI_PAGESIZE);

  //fprintf(stderr,"Finished handling segv for %d\n",getpid());

  timers::stopTimer(timers::SEGV);
}

void memory::commit(int pid)
{
  if(_states == nullptr){
    return;
  }

  timers::startTimer(timers::COMMIT);

  int *numStates = (int*)_states;

  //fprintf(stderr,"Committing %d pages for pid %d\n",*numStates,pid);

  pagestate *real_states = (pagestate*)(numStates+1);

  size_t bytesProtected = 0;
  size_t bytesModified = 0;

  for(int c = 0; c < *numStates; c++){
    if(real_states[c]._active == false){
      continue;
    }

    void *addr = real_states[c]._addr;
    pagestate *state = &real_states[c];

    MUST(state != nullptr);

    char *current = (char*)addr;
    char *twin = (char*)state->_twin;

    if(twin == nullptr){
      continue;
    }

    int incSize = 16384;
    char *share = _persistentMemory + (current-_slabStart);
    
    for(int i = 0; i < TMI_PAGESIZE; i += incSize){
      bytesProtected += incSize;
      if(memcmp(&current[i],&twin[i],4096) == 0){
	continue;
      }
      
      for(int c = i; c < i + incSize; c++){
	if(current[c] != twin[c]){
	  share[c] = current[c];
	  ++bytesModified;
	}
      }
    }

    madvise((void*)addr,TMI_PAGESIZE,MADV_DONTNEED);
    mprotect((void*)addr,TMI_PAGESIZE,PROT_READ);
  }

  //fprintf(stderr,"Bytes Protected: %lu, Bytes Modified: %lu\n",bytesProtected,bytesModified);

  //fprintf(stderr,"Finished committing\n");
  timers::stopTimer(timers::COMMIT);

  //timers::printTimers();
}
#endif

void* memory::adjust_slab_memory(size_t size)
{
  if(!_initialized){
    init();
  }

  void* mem = _slabPos;
  _slabPos += size;

  return mem;
}

void* memory::get_mmap_memory(size_t size)
{
  if(!_initialized){
    init();
  }

  if(size % TMI_PAGESIZE != 0){
    size = ((size / TMI_PAGESIZE) + 1) * TMI_PAGESIZE;
  }

  void* mem = nullptr;
  _mmapPos -= size;
  mem = _mmapPos;

  return mem;
}

#define MAGIC_IN_UPDATE 0xDEADBEEF
#define MAGIC_SET 0xCAFEBABE

typedef struct shared_state_t{
  std::atomic<long> magic;
  void *realObject;
} shared_state;

// Get a pointer to an internal memory location (i.e. always shared
// state)
// This is used for cosntructs like locks that we don't want to 
// ever be private to some process
void* memory::get_shared(void *base, size_t size, int type)
{
  shared_state *shared = (shared_state*)base;
  
  int magic = shared->magic.load();

  if(magic == MAGIC_SET){
    return shared->realObject;
  }

  if(magic == MAGIC_IN_UPDATE){
    do{
      magic = shared->magic.load();
    }while(magic != MAGIC_SET);
    MUST(magic == MAGIC_SET);
    return shared->realObject;
  }

  int oldValue = shared->magic.exchange(MAGIC_IN_UPDATE);

  if(oldValue == MAGIC_IN_UPDATE || oldValue == MAGIC_SET){
    // Recurse and end up in one of the other cases if we didn't win
    return get_shared(base,size,type);
  }

  MUST(oldValue != MAGIC_IN_UPDATE && oldValue != MAGIC_SET);
  void *sharedMem = tmi_internal_alloc(size);

  if(type == 1){
    tmiutil::init_mutex((pthread_mutex_t*)sharedMem);
  }else if(type == 2){
    tmiutil::init_cond((pthread_cond_t*)sharedMem);
  }else if(type == 3){
    tmiutil::init_spinlock((pthread_spinlock_t*)sharedMem);
  }

  shared->realObject = sharedMem;
  oldValue = shared->magic.exchange(MAGIC_SET);
  MUST(oldValue == MAGIC_IN_UPDATE);
  
  return shared->realObject;
}

char *memory::_persistentMemory = 0;

char *memory::_slabStart = 0;
char *memory::_slabEnd = 0;
char *memory::_slabPos = 0;
char *memory::_mmapPos = 0;
char *memory::_mmapPos2 = 0;
int memory::_backingFd = 0;

bool memory::_initialized = false;

pagestate *memory::_states = 0;

extern "C"{

void* get_shared(void* base,size_t size, int type)
{
  return memory::get_shared(base,size,type);
}

void tmi_commit()
{
#ifdef TMI_PROTECT
  memory::commit(getpid());
#endif
}

void* adjust_TMI_slab_memory(size_t size)
{
  return memory::adjust_slab_memory(size);
}

void* get_TMI_mmap_memory(size_t size)
{
  return memory::get_mmap_memory(size);
}

int return_TMI_mmap_memory(void *ptr, size_t size)
{
  return 1;
}

int is_protectable(void *address)
{
  return memory::isProtectable(address);
}

void protect_address(void *address)
{
  printf("Attempting to protect address: %p\n",address);
  memory::protectAddress(address);
}

void protect_all()
{
  printf("Protecting all allocated addresses\n");
  memory::protectAll();
}

void segvhandle(int sig, siginfo_t *si, void *unused)
{
  if(si->si_code == SEGV_ACCERR){
    memory::handleSegv(sig,si->si_addr);
  }else{
    fprintf(stderr,"Other segv error at address %p on %d!\n",si->si_addr,gettid());

    while(1){}

    tmi_exit();
  }
}

}
