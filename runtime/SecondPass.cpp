#include <cstdio>
#include <vector>
#include <mutex>
#include <unordered_map>
#include <map>
#include <string>
#include <stdio.h>

#include "MemArith.h"
#include "xthread.h"
#include "GetGlobal.h"
#include "CacheLine.h"

class MallocInformation
{
public:
  unsigned int id;
  void * start;
  size_t size;
  unsigned int offsetLength;
  std::unordered_map<unsigned int, bool> offsets;
};

class AllMallocInformation
{
public:
  std::map<void *, MallocInformation *> findMap;
  std::vector<MallocInformation *> allMallocs;
  unsigned int currentIndex;
  unsigned int size;
  AllMallocInformation()
  {
    FILE *fp = fopen("__record___malloc.log", "r");
    if (fp == NULL)
    {
      printf("Error no first pass malloc profile file found!\n");
      exit(-1);
    }
    unsigned int id;
    MallocInformation *currentMalloc;
    while(fscanf(fp, "%u", &id))
    {
      currentMalloc = new MallocInformation;
      currentMalloc->id = id;
      fscanf(fp, "%u", &(currentMalloc->offsetLength));
      unsigned int offset;
      for(unsigned int i=0; i<currentMalloc->offsetLength; i++)
      {
        fscanf(fp, "%u", &offset);
        currentMalloc->offsets[offset] = true;
      }
      allMallocs.push_back(currentMalloc);
    }
    currentIndex = 0;
    size = allMallocs.size();
  }
  void push_new_malloc(unsigned int id, void * start, size_t size)
  {
    if(currentIndex >= size)return;
    else if(id<allMallocs[currentIndex]->id)return;
    else if (id > allMallocs[currentIndex]->id)printf("Error while pushing new malloc");
    else {
      allMallocs[currentIndex]->start = start;
      allMallocs[currentIndex]->size  = size;
      findMap[start] = allMallocs[currentIndex];
      currentIndex += 1;
    }
  }
  bool isFalseSharingAddress(void *address)
  {
    auto it = findMap.upper_bound(address);
    if(it==findMap.begin())return false;
    it--;
    MallocInformation * currentMalloc = it->second;
    if((currentMalloc->start <= address) && (address < (currentMalloc->start)+(currentMalloc->size)))
    {
      //do some other checking
      unsigned int currentOffset = (long)address - (long)(currentMalloc->start);
      auto foundIt = currentMalloc->offsets.find(currentOffset);
      if(foundIt==currentMalloc->offsets.end())return false;
      else return true;
    }
    else return false;
  }
};

extern "C" {

void *globalStart, *globalEnd;
void *heapStart = (void *) ((uintptr_t) 1 << 63), *heapEnd;

void initializer(void) __attribute__((constructor));
void finalizer(void) __attribute__((destructor));
void *malloc(size_t size) noexcept;
void *__libc_malloc(size_t size);
void __libc_free(void *ptr);
// void free(void *ptr);
void handle_access(uintptr_t addr, uint64_t func_id, uint64_t inst_id,
                   size_t size, bool is_write);

void store_16bytes(uintptr_t addr, uint64_t func_id, uint64_t inst_id) {
    handle_access(addr, func_id, inst_id, 16, true);
}
void store_8bytes(uintptr_t addr, uint64_t func_id, uint64_t inst_id) {
    handle_access(addr, func_id, inst_id, 8, true);
}
void store_4bytes(uintptr_t addr, uint64_t func_id, uint64_t inst_id) {
    handle_access(addr, func_id, inst_id, 4, true);
}
void store_2bytes(uintptr_t addr, uint64_t func_id, uint64_t inst_id) {
    handle_access(addr, func_id, inst_id, 2, true);
}
void store_1bytes(uintptr_t addr, uint64_t func_id, uint64_t inst_id) {
    handle_access(addr, func_id, inst_id, 1, true);
}
void load_16bytes(uintptr_t addr, uint64_t func_id, uint64_t inst_id) {
    handle_access(addr, func_id, inst_id, 16, false);
}
void load_8bytes(uintptr_t addr, uint64_t func_id, uint64_t inst_id) {
    handle_access(addr, func_id, inst_id, 8, false);
}
void load_4bytes(uintptr_t addr, uint64_t func_id, uint64_t inst_id) {
    handle_access(addr, func_id, inst_id, 4, false);
}
void load_2bytes(uintptr_t addr, uint64_t func_id, uint64_t inst_id) {
    handle_access(addr, func_id, inst_id, 2, false);
}
void load_1bytes(uintptr_t addr, uint64_t func_id, uint64_t inst_id) {
    handle_access(addr, func_id, inst_id, 1, false);
}
}

AllMallocInformation * allMallocInformation;

unsigned int mallocId = 0;

class MallocHookDeactivator {
public:
    MallocHookDeactivator() noexcept { current->malloc_hook_active = false; }

    ~MallocHookDeactivator() noexcept { current->malloc_hook_active = true; }
};

void initializer(void) {
#ifdef DEBUG
    printf("Initializing...\n");
#endif
    xthread::getInstance().initialize();
    getGlobalRegion(&globalStart, &globalEnd);
    printf("globalStart = %p, globalEnd = %p\n", globalStart, globalEnd);
    allMallocInformation = new AllMallocInformation;
    current->malloc_hook_active = true;
}

void finalizer(void) {
    // RAII deactivate malloc hook so that we can use printf (and malloc) below.
    MallocHookDeactivator deactiv;
#ifdef DEBUG
    printf("Finalizing...\n");
#endif
}

void *my_malloc_hook(size_t size, const void *caller) {
    // RAII deactivate malloc hook so that we can use malloc below.
    MallocHookDeactivator deactiv;
    void *alloced = malloc(size);
    //mallocStarts.push_back(alloced);
    //mallocOffsets.push_back(size);
    heapStart = std::min(heapStart, alloced);
    heapEnd = std::max(heapEnd, alloced + size);
#ifdef DEBUG
    // printf("malloc(%lu) called from %p returns %p\n", size, caller, alloced);
    // printf("heapStart = %p, heapEnd = %p\n", heapStart, heapEnd);
#endif
    // Record that range of address.
    if (getThreadIndex() == 0)
    {
        allMallocInformation->push_new_malloc(mallocId, alloced, size);
    }
    return alloced;
}

void *malloc(size_t size) noexcept {
    void *caller = __builtin_return_address(0);
    if (current && current->malloc_hook_active)
        return my_malloc_hook(size, caller);
    return __libc_malloc(size);
}


void my_free_hook(void *ptr, const void *caller) {
    // RAII deactivate malloc hook so that we can use free below.
    MallocHookDeactivator deactiv;
    free(ptr);
#ifdef DEBUG
    printf("free(%p) called from %p\n", ptr, caller);
#endif
}

void free(void *ptr) {
    void *caller = __builtin_return_address(0);
    // if (current && current->malloc_hook_active)
    // return my_free_hook(ptr, caller);
    return __libc_free(ptr);
}

void handle_access(uintptr_t addr, uint64_t func_id, uint64_t inst_id,
                   size_t size, bool is_write) {
    MallocHookDeactivator deactiv;
    auto addr_ptr = (void *) addr;
    bool is_heap = (addr_ptr >= heapStart && addr_ptr < heapEnd);
    bool is_global = (addr_ptr >= globalStart && addr_ptr < globalEnd);
    if (!is_heap && !is_global)
        return;
    if (is_heap) {
        try {
            allMallocInformation->isFalseSharingAddress((void *)addr);
        }
        catch (std::invalid_argument&) {
            return;
        }
    } else
        return;
}

// Intercept the pthread_create function.
int pthread_create(pthread_t *tid, const pthread_attr_t *attr,
                   void *(*start_routine)(void *), void *arg) {
    MallocHookDeactivator deactiv;
    int res = xthread::getInstance().thread_create(tid, attr, start_routine, arg);
    return res;
}
