#ifndef LOGGINGTHREAD_H
#define LOGGINGTHREAD_H

#include <pthread.h>
#include <mutex>

typedef void * threadFunction (void *);

const size_t LOG_SIZE = 1 << 19;

struct RWRecord {
    uintptr_t addr;
    uint16_t func_id, inst_id, size;
    bool is_write;
    RWRecord(uintptr_t _addr, uint16_t _func_id, uint16_t _inst_id, uint16_t _size, bool _is_write):
        addr(_addr), func_id(_func_id), inst_id(_inst_id), size(_size), is_write(_is_write) {}
    RWRecord() = default;
};

struct Thread {
    // Buffer for read/write records.
    RWRecord outputBuf[LOG_SIZE];
    size_t output_n;
    // File handle
    FILE *buffer_f;
    // Results of pthread_self
    pthread_t self;
    // The following is the parameter about starting function. 
    threadFunction * startRoutine;
    void * startArg;
    // We used this to record the stack range
    // void * stackBottom;
    // void * stackTop;
    // index of this thread object.
    int index;
    // True: the thread index is free.
    bool available;
    // True: malloc will call our malloc_hook.
    bool malloc_hook_active;

    void flush_log();
    void log_load_store(uintptr_t addr, uint16_t func_id, uint16_t inst_id, uint16_t size, bool is_write);
};

#endif
