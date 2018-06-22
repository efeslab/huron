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
#include <algorithm>
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

        explicit PerAddr(size_t _id = 0, size_t _size = 0)
                : id(_id), size(_size) {}
    };

    struct PerBt {
        uintptr_t addr;
        size_t id, size;

        explicit PerBt(uintptr_t _addr, size_t _id, size_t _size)
                : addr(_addr), id(_id), size(_size) {}

        bool operator<(const PerBt &rhs) const {
            return addr < rhs.addr;
        }
    };

    // This is an (ordered) map because we need to query lower bound for incoming access addresses.
    std::map<uintptr_t, PerAddr> *data_alive;
    std::unordered_map<std::vector<void *>, std::vector<PerBt>> data_total;
    std::shared_timed_mutex mutex;
    AddrSeg heap;
    size_t id;
public:
    MallocInfo() : data_total(), heap(~0LU, 0), id(0) {
        HookDeactivator deactiv;
        data_alive = new std::map<uintptr_t, PerAddr>;
    }

    void dump(const char *path) const {
        FILE *file = fopen(path, "w");
        assert(file);
        SymbolCache scache;
        std::vector<PerBt> all_records;
        for (const auto &p: data_total) {
            for (const auto &per_bt: p.second)
                fprintf(file, "%lu,%p,%lu\n", per_bt.id, (void *) per_bt.addr, per_bt.size);
            scache.backtrace_symbols_fd(p.first, file);
            all_records.insert(all_records.end(), p.second.begin(), p.second.end());
        }
        std::sort(all_records.begin(), all_records.end());
        for (const auto &per_bt: all_records)
            fprintf(file, "%lu,%p,%lu\n", per_bt.id, (void *) per_bt.addr, per_bt.size);
    }

    void insert(uintptr_t start, size_t size) {
        // There's only one writer (thread 0), so we're doing this to avoid locks.
        if (data_alive->find(start) == data_alive->end()) {
            auto *copy = new std::map<uintptr_t, PerAddr>(*data_alive);
            copy->emplace(start, PerAddr(id, size));
            // No one can modify data_alive between the following 2 lines, so we're safe.
            auto *old = data_alive;
            data_alive = copy;  // atomic!
            delete old;
        }
        static void *bt_buf[1000];
        int bt_size = backtrace(bt_buf, 1000);
        std::vector<void *> bt(bt_buf, bt_buf + bt_size);
        heap.insert(AddrSeg(start, start + size));
        data_total[bt].emplace_back(start, id, size);
        id++;
    }

    bool erase(uintptr_t addr) {
        // There's only one writer (thread 0), so we're doing this to avoid locks.
        if (data_alive->find(addr) == data_alive->end())
            return false;
        auto *copy = new std::map<uintptr_t, PerAddr>(*data_alive);
        auto it = copy->find(addr);  // it's a different container, so do find again.
        assert(it != copy->end());
        size_t size = it->second.size;
        copy->erase(it);
        // No one can modify data_alive between the following 2 lines, so we're safe.
        auto *old = data_alive;
        data_alive = copy;  // atomic!
        delete old;
        heap.shrink(AddrSeg(addr, size));
        return true;
    }

    inline bool contain(uintptr_t addr) {
        return heap.contain(addr);
    }

    bool find_id_offset(uintptr_t addr, size_t &id, size_t &offset) {
        // Find the first starting address greater than `addr`, then it--
        // to get where `addr` falls in.
        auto it = data_alive->upper_bound(addr);
        if (it != data_alive->begin()) {
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
