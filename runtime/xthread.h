
// -*- C++ -*-

/*
Allocate and manage thread index.
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.
  
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  
  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/*
 * @file   xthread.h
 * @brief  Managing the thread creation, etc.
 * @author Tongping Liu <http://www.cs.umass.edu/~tonyliu>
 */

#ifndef _XTHREAD_H_
#define _XTHREAD_H_

#include <cassert>
#include <cstdio>
#include <new>
#include <mutex>
#include <sys/types.h>
#include <dlfcn.h>
#include <cstring>

#include "LoggingThread.h"

const int MAX_THREADS = 64;

__thread Thread *current;

inline int getThreadIndex() {
    return current->index;
}

int __internal_pthread_create(pthread_t *t1, const pthread_attr_t *t2,
                              void *(*t3)(void *), void *t4) {
    typedef int (*p_create_t)(pthread_t *, const pthread_attr_t *, void *(*)(void *), void *);
    static p_create_t _pthread_create_ptr;
    if (_pthread_create_ptr == nullptr) {
        void *lib_handle = dlopen("libpthread.so.0", RTLD_NOW | RTLD_GLOBAL | RTLD_NOLOAD);
        if (lib_handle == nullptr) {
            fprintf(stderr, "Unable to load libpthread.so.0\n");
            exit(2);
        }
        _pthread_create_ptr = (p_create_t) dlsym(lib_handle, "pthread_create");
        assert(_pthread_create_ptr);
    }
    return _pthread_create_ptr(t1, t2, t3, t4);
}

class xthread {
private:
    xthread() = default;

public:
    static xthread &getInstance() {
        static char buf[sizeof(xthread)];
        static auto *theOneTrueObject = new(buf) xthread();
        return *theOneTrueObject;
    }

    /// @brief Initialize the system.
    void initialize() {
        _aliveThreads = 0;
        _threadIndex = 0;

        _totalThreads = MAX_THREADS;

        pthread_mutex_init(&_lock, nullptr);

        // Shared the threads information. 
        memset(&_threads, 0, sizeof(_threads));

        // Initialize all mutex.
        Thread *thisThread;

        for (int i = 0; i < _totalThreads; i++) {
            thisThread = &_threads[i];

            thisThread->available = true;
        }

        // Allocate the threadindex for current thread.
        initInitialThread();
    }

    // Initialize the first threadd
    void initInitialThread() {
        int tindex;

        // Allocate a global thread index for current thread.
        tindex = allocThreadIndex();

        // First, xdefines::MAX_ALIVE_THREADS is too small.
        if (tindex == -1) {
            fprintf(stderr, "The alive threads is larger than xefines::MAX_THREADS larger!!\n");
            assert(0);
        }

        // Get corresponding Thread structure.
        current = getThreadInfo(tindex);

        current->index = tindex;
        current->self = pthread_self();
        current->malloc_hook_active = false;
    }

    Thread *getThreadInfo(int index) {
        assert(index < _totalThreads);
        return &_threads[index];
    }

    // Allocate a thread index under the protection of global lock
    int allocThreadIndex() {
        int index = -1;

        if (_aliveThreads >= _totalThreads) {
            fprintf(stderr, "Set xdefines::MAX_THREADS to larger. _alivethreads %d totalthreads %d", _aliveThreads,
                    _totalThreads);
            abort();
        }

        int origindex = _threadIndex;
        //		fprintf(stderr, "threadindex %d\n", _threadIndex);
        Thread *thread;
        while (true) {
            thread = getThreadInfo(_threadIndex);
            if (thread->available) {
                thread->available = false;
                index = _threadIndex;

                // A thread is counted as alive when its structure is allocated.
                _aliveThreads++;

                _threadIndex = (_threadIndex + 1) % _totalThreads;
                break;
            } else {
                _threadIndex = (_threadIndex + 1) % _totalThreads;
            }

            // It is impossible that we search the whole array and we can't find
            // an available slot.
            assert(_threadIndex != origindex);
        }
        return index;
    }

    /// Create the wrapper 
    /// @ Intercepting the thread_creation operation.
    int thread_create(pthread_t *tid, const pthread_attr_t *attr, threadFunction *fn, void *arg) {
        void *ptr = nullptr;
        int tindex;
        int result;

        // Lock and record
        global_lock();

        // Allocate a global thread index for current thread.
        tindex = allocThreadIndex();

        // First, xdefines::MAX_ALIVE_THREADS is too small.
        if (tindex == -1) {
            fprintf(stderr, "The alive threads is larger than xefines::MAX_THREADS larger!!\n");
            assert(0);
        }

        // Get corresponding Thread structure.
        Thread *children = getThreadInfo(tindex);

        children->index = tindex;
        children->startRoutine = fn;
        children->startArg = arg;
        children->malloc_hook_active = false;

        global_unlock();

        result = __internal_pthread_create(tid, attr, startThread, (void *) children);

        // Set up the thread index in the local thread area.
        return result;
    }


    // @Global entry of all entry function.
    static void *startThread(void *arg) {
        void *result;

        current = (Thread *) arg;
        //    current->tid = gettid();
        current->self = pthread_self();

        // from the TLS storage.
        result = current->startRoutine(current->startArg);

        // Decrease the alive threads
        xthread::getInstance().removeThread(current);

        return result;
    }

    void flush_all_thread_logs() {
        for (auto &_thread : _threads)
            _thread.flush_log();
    }

private:
    /// @brief Lock the lock.
    void global_lock() {
        pthread_mutex_lock(&_lock);
    }

    /// @brief Unlock the lock.
    void global_unlock() {
        pthread_mutex_unlock(&_lock);
    }

    void removeThread(Thread *thread) {
        //  fprintf(stderr, "remove thread %p with thread index %d\n", thread, thread->index);
        global_lock();

        current->available = true;
        --_aliveThreads;

        if (_aliveThreads == 0) {
            // isMultithreading = false;
        }

        global_unlock();
    }

    pthread_mutex_t _lock;
    int _threadIndex;
    int _aliveThreads;
    int _totalThreads;
    // Total threads we can support is MAX_THREADS
    Thread _threads[MAX_THREADS];
};

#endif

