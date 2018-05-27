//
// Created by yifanz on 5/27/18.
//

#ifndef RUNTIME_MALLOCINFO_H
#define RUNTIME_MALLOCINFO_H

#include <map>

struct MallocIdSize {
    size_t id, size;

    MallocIdSize(size_t _id, size_t _size) : id(_id), size(_size) {}
};

class MallocInfo {
    // This is an (ordered) map because we need to query lower bound for incoming access addresses.
    std::map<uintptr_t, MallocIdSize> data;
    size_t malloc_id;
public:
    MallocInfo() : malloc_id(0) {}

    void dump() {
        for (auto &p : data)
            printf("alloc(%lu): %p+%lu\n", p.second.id, (void *) p.first, p.second.size);
    }

    void insert(uintptr_t start, size_t size) {
        data.emplace(start, MallocIdSize(malloc_id++, size));
    }
};

#endif //RUNTIME_MALLOCINFO_H
