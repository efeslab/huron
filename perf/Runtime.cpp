#include <iostream>
#include <mutex>

#include "Segment.h"
#include "ProcMaps.h"
#include "xthread.h"

extern "C" {
void initializer(void) __attribute__((constructor));
void finalizer(void) __attribute__((destructor));
}

AddrSeg text;

void initializer(void) {
    text = getTextRegion();
    std::ios_base::Init mInitializer;
    xthread::getInstance().initMainThread();
}

void finalizer(void) {
    xthread::getInstance().stopMainThread();
}

// Intercept the pthread_create function.
int pthread_create(pthread_t *tid, const pthread_attr_t *attr,
                   void *(*start_routine)(void *), void *arg) {
    if (current && current->all_hooks_active) {
        HookDeactivator deactiv;
        int res = xthread::getInstance().thread_create(tid, attr, start_routine, arg);
        return res;
    } else
        return __internal_pthread_create(tid, attr, start_routine, arg);
}
