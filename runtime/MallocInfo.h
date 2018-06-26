//
// Created by yifanz on 5/27/18.
//

#ifndef RUNTIME_MALLOCINFO_H
#define RUNTIME_MALLOCINFO_H

#include <cstdio>
#include <map>
#include <sstream>
#include <utility>
#include <vector>
#include <shared_mutex>
#include <execinfo.h>
#include <cassert>
#include "Segment.h"
#include "LoggingThread.h"
#include "SymbolCache.h"

namespace std {
    template<class T>
    struct hash<std::vector<T>> {
        std::size_t operator()(std::vector<T> const &vec) const {
            std::size_t seed = vec.size();
            std::hash<T> hasher;
            for (auto &i : vec) {
                seed ^= hasher(i) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            }
            return seed;
        }
    };
}

class MallocInfo {
    struct PerAddr {
        size_t id, size;
        std::vector<void *> bt;

        explicit PerAddr(size_t _id = 0, size_t _size = 0, std::vector<void *> _bt = std::vector<void *>())
                : id(_id), size(_size), bt(std::move(_bt)) {}
    };

    struct PerBt {
        uintptr_t addr;
        size_t id, size;

        explicit PerBt(uintptr_t _addr, size_t _id, size_t _size)
                : addr(_addr), id(_id), size(_size) {}
    };

    // This is an (ordered) map because we need to query lower bound for incoming access addresses.
    std::map<uintptr_t, PerAddr> data_alive;
    std::unordered_map<std::vector<void *>, std::vector<PerBt>> data_total;
    std::shared_timed_mutex mutex;
    AddrSeg heap;
    size_t id;
public:
    MallocInfo() : data_total(), heap(~0LU, 0), id(0) {
        HookDeactivator deactiv;
    }

    void dump(const char *path) {
        FILE *file = fopen(path, "w");
        assert(file);
        SymbolCache scache;
        for (const auto &p: data_total) {
            for (const auto &per_bt: p.second)
                fprintf(file, "%lu,%p,%lu\n", per_bt.id, (void *) per_bt.addr, per_bt.size);
            scache.backtrace_symbols_fd(p.first, file);
        }
    }

    void insert(uintptr_t start, size_t size) {
        // This function itself is single threaded,
        // so not all operations need protection with lock.
        static void *bt_buf[1000];
        int bt_size = backtrace(bt_buf, 1000);
        std::vector<void *> bt(bt_buf, bt_buf + bt_size);
        mutex.lock();
        data_alive[start] = PerAddr(id, size, bt);
        mutex.unlock();
        heap.insert(AddrSeg(start, start + size));
        data_total[bt].emplace_back(start, id, size);
        id++;
    }

    void insert(uintptr_t start, size_t size, uint64_t func_id, uint64_t inst_id) {
        // This function itself is single threaded,
        // so not all operations need protection with lock.
        static void *bt_buf[1000];
        int bt_size = backtrace(bt_buf, 1000);
        std::vector<void *> bt(bt_buf, bt_buf + bt_size);
        mutex.lock();
        data_alive[start] = PerAddr(id, size, bt);
        mutex.unlock();
        heap.insert(AddrSeg(start, start + size));
        data_total[bt].emplace_back(start, id, size);
        id++;
        FILE *mallocRuntimeIDs = fopen("mallocRuntimeIDs.txt", "a");
        fprintf(mallocRuntimeIDs,"%lu,%lu,%lu,%lu,%lu\n",id-1,start,size,func_id,inst_id);
        fclose(mallocRuntimeIDs);
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

    bool find_id_offset(uintptr_t addr, size_t &id, size_t &offset) {
        // Find the first starting address greater than `addr`, then it--
        // to get where `addr` falls in.
        std::shared_lock<std::shared_timed_mutex> ul(mutex);
        auto it = data_alive.upper_bound(addr);
        if (it != data_alive.begin()) {
            it--;
            if (it->first + it->second.size > addr) {
                id = it->second.id;
                offset = addr - it->first;
                return true;
            }
        }
        return false;
    }
};

#endif //RUNTIME_MALLOCINFO_H
