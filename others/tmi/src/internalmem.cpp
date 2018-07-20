#include "internalmem.hpp"
#include "tmiutil.hpp"
#include "hooks.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/mman.h>
#include <unistd.h>

char *internalmemory::_start = nullptr;
char *internalmemory::_pos = nullptr;
char *internalmemory::_end = nullptr;

bool internalmemory::_initialized = false;

int internalmemory::isInternal(void *addr)
{
  return (size_t)addr >= (size_t)_start &&
    (size_t)addr < (size_t)_end;
}

void internalmemory::init()
{ 
  if(_initialized){
    return;
  }

  _initialized = true;

  _start = (char*)mmap(0,INTERNAL_HEAP_SIZE,PROT_READ | PROT_WRITE,
		       MAP_SHARED | MAP_ANONYMOUS,0,0);

  _pos = _start;
  _end = _start + INTERNAL_HEAP_SIZE;
  
  if(_start == MAP_FAILED){
    perror("mmap:");
  }

  fprintf(stderr,"Initialized internal mem\n");
}

void internalmemory::cleanup()
{
  //fprintf(stderr,"Total internal heap used: %lu\n",(_pos - _start));
}

void* internalmemory::alloc(size_t size)
{
  if(!_initialized){
    init();
  }

  MUST(_pos + size <= _end);
  void *block = (void*)_pos;
  _pos += size;

  return block;
}

extern "C"{
void * tmi_internal_alloc(size_t size)
{
  return internalmemory::alloc(size);
}

void tmi_internal_dealloc(void *p)
{
  // Noop for now
}
}
