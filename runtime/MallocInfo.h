//
// Created by yifanz on 5/27/18.
//

#ifndef RUNTIME_MALLOCINFO_H
#define RUNTIME_MALLOCINFO_H

#include <cstdio>
#include <map>
#include <sstream>
#include <vector>
#include <shared_mutex>
#include <execinfo.h>
#include <cassert>
#include "Segment.h"
#include "LoggingThread.h"
#include "SymbolCache.h"

struct MallocIdSize {
    size_t id, size;

    explicit MallocIdSize(size_t _id = 0, size_t _size = 0) : id(_id), size(_size) {}
};

class MallocInfo {
    // This is an (ordered) map because we need to query lower bound for incoming access addresses.
    std::map<uintptr_t, MallocIdSize> data_alive;
    std::shared_timed_mutex mutex;
    SymbolCache scache;
    AddrSeg heap;
    FILE *file;
    size_t id;
public:
    static const size_t nfound = ~0LU;

    MallocInfo() : mutex(), heap(~0LU, 0), id(0) {
        HookDeactivator deactiv;
        file = fopen("mallocIds.csv", "w");
        assert(file);
    }

    void insert(uintptr_t start, size_t size) {
        // This function itself is single threaded,
        // so not all operations need protection with lock.
        mutex.lock();
        data_alive[start] = MallocIdSize(id, size);
        mutex.unlock();
        heap.insert(AddrSeg(start, start + size));

        fprintf(file, "%lu,%p,%lu\n", id++, (void *) start, size);
        static void *bt_buf[1000];
        int bt_size = backtrace(bt_buf, 1000);
        scache.backtrace_symbols_fd(bt_buf, bt_buf + bt_size, file);
    }

    bool erase(uintptr_t addr) {
        std::unique_lock<std::shared_timed_mutex> ul(mutex);
        auto it = data_alive.find(addr);
        if (it == data_alive.end())
            return false;
        size_t size = it->second.size;
        heap.shrink(AddrSeg(addr, size));
        data_alive.erase(it);
        return true;
    }

    inline bool contain(uintptr_t addr) {
        return heap.contain(addr);
    }

    bool find_id_offset(uintptr_t addr, MallocIdSize &id_offset) {
        // Find the first starting address greater than `addr`, then it--
        // to get where `addr` falls in.
        std::shared_lock<std::shared_timed_mutex> ul(mutex);
        auto it = data_alive.upper_bound(addr);
        if (it != data_alive.begin()) {
            it--;
            if (it->first + it->second.size > addr) {
                id_offset = MallocIdSize(it->second.id, addr - it->first);
                return true;
            }
        }
        return false;
    }

    ~MallocInfo() {
        if (file)
            fclose(file);
    }
};

#endif //RUNTIME_MALLOCINFO_H
