//
// Created by yifanz on 5/27/18.
//

#ifndef RUNTIME_MALLOCINFO_H
#define RUNTIME_MALLOCINFO_H

#include <map>
#include <sstream>
#include <vector>

struct MallocIdSize {
    size_t id, size;

    explicit MallocIdSize(size_t _id = 0, size_t _size = 0) : id(_id), size(_size) {}
};

class MallocInfo {
    // This is an (ordered) map because we need to query lower bound for incoming access addresses.
    std::unordered_map<uintptr_t, MallocIdSize> data_alive;
    std::vector<std::pair<uintptr_t, size_t>> data_total;
    FILE *file;
public:
    static const size_t nfound = ~0UL;

    MallocInfo() {
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
    }

    size_t get_size(uintptr_t addr) {
        auto it = data_alive.find(addr);
        return it == data_alive.end() ? nfound : it->second.size;
    }

    ~MallocInfo() {
        if (file)
            fclose(file);
    }
};

#endif //RUNTIME_MALLOCINFO_H
