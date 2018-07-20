#pragma once

#include "tmi.h"

#include <cstdlib>
#include <pthread.h>

extern "C" void* tmi_internal_alloc(size_t size);
extern "C" void tmi_internal_dealloc(void *p);

class internalmemory{
  static char *_start;
  static char *_pos;
  static char *_end;
 
  static bool _initialized;

public:
  static const int INTERNAL_HEAP_SIZE = 1073741824;

  static int isInternal(void *addr);
  static void init();
  static void cleanup();
  static void initThreads();
  static void* alloc(size_t size);
};
