//
// Created by yifanz on 6/15/18.
//

#ifndef RUNTIME_SYMBOLCACHE_H
#define RUNTIME_SYMBOLCACHE_H

#include <unordered_map>
#include <cstddef>
#include <vector>
#include <execinfo.h>
#include <cxxabi.h>
#include <memory>

class SymbolCache {
    std::unordered_map<void *, std::string> addresses;

    std::string demangle(const char *const symbol) {
        std::string symbol_str(symbol);
        size_t begin = symbol_str.find('(');
        if (begin == std::string::npos)
            return symbol;
        size_t end = symbol_str.find('+', begin);
        if (end == std::string::npos)
            return symbol;
        symbol_str = symbol_str.substr(begin + 1, end - begin - 1);
        const std::unique_ptr<char, decltype(&std::free)> demangled(
                abi::__cxa_demangle(symbol_str.c_str(), 0, 0, 0), &std::free);
        return demangled ? demangled.get() : symbol_str;
    }

public:
    void insert_range(const std::vector<void*> &bt) {
        std::vector<void *> not_found;
        for (const auto &ptr: bt) {
            auto it = addresses.find(ptr);
            if (it == addresses.end())
                not_found.push_back(ptr);
        }
        char **string_bufs = backtrace_symbols(not_found.data(), (int) not_found.size());
        for (size_t i = 0; i < not_found.size(); i++)
            addresses.emplace(not_found[i], demangle(string_bufs[i]));
        free(string_bufs);
    }

    void backtrace_symbols_fd(const std::vector<void*> &bt, FILE *file) {
        insert_range(bt);
        for (const auto &ptr: bt)
            fprintf(file, "%s\n", addresses[ptr].c_str());
    }
};

#endif //RUNTIME_SYMBOLCACHE_H
