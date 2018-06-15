//
// Created by yifanz on 6/15/18.
//

#ifndef RUNTIME_SYMBOLCACHE_H
#define RUNTIME_SYMBOLCACHE_H

#include <unordered_map>
#include <cstddef>
#include <vector>
#include <execinfo.h>

class SymbolCache {
    std::unordered_map<void *, std::string> addresses;
public:
    void insert_range(void **begin, void **end) {
        std::vector<void *> not_found;
        for (; begin != end; begin++) {
            auto it = addresses.find(*begin);
            if (it == addresses.end())
                not_found.push_back(*begin);
        }
        char **string_bufs = backtrace_symbols(not_found.data(), (int)not_found.size());
        for (size_t i = 0; i < not_found.size(); i++)
            addresses.emplace(not_found[i], string_bufs[i]);
        free(string_bufs);
    }

    void backtrace_symbols_fd(void **begin, void **end, FILE *file) {
        insert_range(begin, end);
        for (; begin != end; begin++)
            fprintf(file, "%s\n", addresses[*begin].c_str());
    }
};

#endif //RUNTIME_SYMBOLCACHE_H
