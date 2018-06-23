
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
#include <cstdlib>

#include "LoggingThread.h"
#include "LibFuncs.h"

const int MAX_THREADS = 1 << 7;

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

    // Initialize the first threadd
    void initInitialThread() {
        // Insert a thread in the vector and `current`
        _threads.emplace_back(0, nullptr, nullptr);
        current = &_threads.back();
    }

    /// Create the wrapper 
    /// @ Intercepting the thread_creation operation.
    int thread_create(pthread_t *tid, const pthread_attr_t *attr, threadFunction *fn, void *arg) {
        std::lock_guard<std::mutex> lg(_lock);

        if (_threads.size() >= MAX_THREADS) {
            fprintf(stderr, "Set xdefines::MAX_THREADS to larger. _alivethreads %d MAX_THREADS %d",
                    _aliveThreads, MAX_THREADS);
            abort();
        }
        // Insert a thread.
        _threads.emplace_back(_threads.size(), fn, arg);
        Thread *children = &_threads.back();
        // Run it starting from the wrapper.
        _aliveThreads++;
        int result = __internal_pthread_create(tid, attr, startThread, (void *) children);

        return result;
    }

    // @Global entry of all entry function.
    static void *startThread(void *arg) {
        current = (Thread *) arg;
        // Get current from the TLS storage. Start the thread main routine.
        void *result = current->startRoutine(current->startArg);
        // We are done. Remove one thread.
        xthread::getInstance().removeThread();
        return result;
    }

    void flush_all_concat_to(const std::string &output_name) {
        assert(current->index == 0);
        // Ask all threads to stop writing, and append files together.
        for (auto &th: _threads) {
            th.stop_logging();
            std::string cat_cmd = "cat " + th.get_filename() + " >> " + output_name;
            system(cat_cmd.c_str());
            std::string rm_cmd = "rm " + th.get_filename();
            system(rm_cmd.c_str());
        }
        // std::string wc_out_cmd = "wc -l " + output_name;
        // system(wc_out_cmd.c_str());
    }

private:
    void removeThread() {
        std::lock_guard<std::mutex> lg(_lock);
        --_aliveThreads;
    }

    std::mutex _lock;
    std::vector<Thread> _threads;
    int _aliveThreads;
};

#endif

