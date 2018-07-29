//
// Created by yifanz on 7/11/18.
//

#ifndef POSTPROCESS_STATS_H
#define POSTPROCESS_STATS_H

#include <map>
#include <fstream>
#include "Utils.h"

class FSRankStat {
public:
    explicit FSRankStat(const std::string &path) : stats_stream(path) {}

    void emplace(size_t fs, int mloc) {
        fs_mloc_ordering.emplace(fs, mloc);
    }

    void print() {
        for (auto it = fs_mloc_ordering.rbegin(); it != fs_mloc_ordering.rend(); it++)
            stats_stream << '#' << it->first << '@' << it->second << '\n';
    }

private:
    std::map<size_t, int> fs_mloc_ordering;
    std::ofstream stats_stream;
};

#endif //POSTPROCESS_STATS_H
