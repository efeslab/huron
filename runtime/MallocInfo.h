//
// Created by yifanz on 5/27/18.
//

#ifndef RUNTIME_MALLOCINFO_H
#define RUNTIME_MALLOCINFO_H

#include <map>
#include <sstream>
#include <vector>
#include "Segment.h"

struct MallocIdSize {
    size_t id, size;

    explicit MallocIdSize(size_t _id = 0, size_t _size = 0) : id(_id), size(_size) {}
};

class MallocInfo {
    // This is an (ordered) map because we need to query lower bound for incoming access addresses.
    std::map<uintptr_t, MallocIdSize> data_alive;
    std::vector<std::pair<uintptr_t, size_t>> data_total;
    AddrSeg heap;
    FILE *file;
public:
    static const size_t nfound = ~0LU;

    MallocInfo(): heap(~0LU, 0) {
        file = fopen("mallocIds.csv", "w");
    }

    void dump() {
        for (size_t i = 0; i < data_total.size(); i++)
            fprintf(file, "%lu,%p,%lu\n", i, (void *) data_total[i].first, data_total[i].second);
    }

    void insert(uintptr_t start, size_t size) {
        size_t id = data_total.size();
        data_alive[start] = MallocIdSize(id, size);
        data_total.emplace_back(start, size);
        heap.insert(AddrSeg(start, start + size));
    }

    bool erase(uintptr_t addr) {
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
