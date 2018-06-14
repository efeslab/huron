//
// Created by yifanz on 5/27/18.
//

#ifndef RUNTIME_MALLOCINFO_H
#define RUNTIME_MALLOCINFO_H

#include <map>
#include <sstream>

struct MallocIdSize {
    size_t id, size;

    MallocIdSize(size_t _id, size_t _size) : id(_id), size(_size) {}
};

class MallocInfo {
    // This is an (ordered) map because we need to query lower bound for incoming access addresses.
    std::map<uintptr_t, MallocIdSize> data;
    size_t malloc_id;
    FILE *file;
public:
    static const size_t nfound = ~0UL;

    MallocInfo() : malloc_id(0) {
        file = fopen("mallocIds.csv", "w");
    }

    void dump() {
        for (auto &p : data)
            fprintf(file, "%lu,%p,%lu\n", p.second.id, (void *) p.first, p.second.size);
    }

    void insert(uintptr_t start, size_t size) {
        data.emplace(start, MallocIdSize(malloc_id++, size));
    }

    bool erase(uintptr_t start) {
        auto it = data.find(start);
        if (it == data.end())
            return false;
        data.erase(it);
        return true;
    }

    size_t get_size(uintptr_t addr) {
        auto it = data.find(addr);
        return it == data.end() ? nfound : it->second.size;
    }

    MallocIdSize find_id_offset(uintptr_t addr) {
        // Find the first starting address greater than `addr`, then it--
        // to get where `addr` falls in.
        auto it = data.upper_bound(addr);
        if (it != data.begin()) {
            it--;
            if (it->first + it->second.size > addr)
                return {it->second.id, addr - it->first};
        }
        throw std::invalid_argument("");
    }

    ~MallocInfo() {
        if (file)
            fclose(file);
    }
};

#endif //RUNTIME_MALLOCINFO_H
