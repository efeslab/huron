
// -*- C++ -*-

/*
Allocate and manage thread index.
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.
  
  This program is distributed contain the hope that it will be useful,
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

#include <iostream>
#include <cassert>
#include <cstdio>
#include <new>
#include <mutex>
#include <sys/types.h>
#include <dlfcn.h>
#include <cstring>
#include <cstdlib>
#include <vector>

#include "LoggingThread.h"

int __internal_pthread_create(pthread_t *t1, const pthread_attr_t *t2, void *(*t3)(void *), void *t4) {
    typedef int (*p_create_t)(pthread_t *, const pthread_attr_t *, void *(*)(void *), void *);
    static p_create_t _pthread_create_ptr;
    if (_pthread_create_ptr == nullptr) {
        void *lib_handle = dlopen("libpthread.so.0", RTLD_NOW | RTLD_GLOBAL | RTLD_NOLOAD);
        if (lib_handle == nullptr) {
            std::cerr << "Unable to load libpthread.so.0\n";
            exit(2);
        }
        _pthread_create_ptr = (p_create_t) dlsym(lib_handle, "pthread_create");
        assert(_pthread_create_ptr);
    }
    return _pthread_create_ptr(t1, t2, t3, t4);
}

const int MAX_THREADS = 1 << 8;

__thread Thread *current;

class xthread {
private:
    xthread() : _aliveThreads(1) {
        // Reserve space, don't allow the vector to move around.
        _threads.reserve(MAX_THREADS);
    }

public:
    static xthread &getInstance() {
        static char buf[sizeof(xthread)];
        static auto *theOneTrueObject = new(buf) xthread();
        return *theOneTrueObject;
    }

    void initMainThread() {
        _threads.emplace_back(0, nullptr, nullptr);
        _threads.back().install_self();
    }

    void stopMainThread() {
        current->remove_self();
        // Ask all threads to stop writing and print.
        std::ofstream stream("__perf_ips.txt");
        for (auto &th: _threads) {
            th.put_list(stream);
        }
        stream.close();
    }

    /// Create the wrapper 
    /// @ Intercepting the thread_creation operation.
    int thread_create(pthread_t *tid, const pthread_attr_t *attr, threadFunction *fn, void *arg) {
        std::lock_guard<std::mutex> lg(_lock);

        if (_threads.size() >= MAX_THREADS) {
            std::cerr << "Set xdefines::MAX_THREADS to larger. _alivethreads "
                      << _aliveThreads << " MAX_THREADS " << MAX_THREADS;
            abort();
        }
        // Insert a thread.
        _threads.emplace_back(_threads.size(), fn, arg);
        Thread *children = &_threads.back();
        // Run it starting from the wrapper.
        _aliveThreads++;
        int result = __internal_pthread_create(tid, attr, startThread, (void *) children);
        _aliveThreads--;
        return result;
    }

    // @Global entry of all entry function.
    static void *startThread(void *arg) {
        return ((Thread *) arg)->run_self();
    }

private:
    std::mutex _lock;
    std::vector<Thread> _threads;
    int _aliveThreads;
};

#endif

