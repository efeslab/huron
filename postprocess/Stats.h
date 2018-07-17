//
// Created by yifanz on 7/11/18.
//

#ifndef POSTPROCESS_STATS_H
#define POSTPROCESS_STATS_H

#include <map>
#include <fstream>
#include <algorithm>
#include "Utils.h"

class TransTableStat {
public:
    explicit TransTableStat(const string &path) : layout_stream(path) {}

    typedef multimap<PC, tuple<size_t, size_t, size_t>> DataType;

    void merge_layout(DataType::iterator begin, DataType::iterator end) {
        all_pcs_layout.insert(begin, end);
    }

    void insert_malloc(const tuple<PC, size_t, size_t> &tup) {
        if (get<2>(tup) != get<1>(tup))
            all_fixed_mallocs.push_back(tup);
    }

    void print() {
        print_malloc();
        print_layout();
    }

private:
    void print_malloc() {
        layout_stream << all_fixed_mallocs.size() << '\n';
        for (const auto &p: all_fixed_mallocs) {
            if (get<0>(p) == PC::null())
                layout_stream << "-1 -1 0 " << get<2>(p) << '\n';
            else
                layout_stream << get<0>(p).func << ' ' << get<0>(p).inst << ' ' << get<1>(p) << ' ' << get<2>(p) << '\n';
        }
    }

    void print_layout() {
        typedef tuple<size_t, size_t, size_t> LineT;
        map<PC, vector<LineT>> pc_grouped;
        for (const auto &pc_layout: all_pcs_layout)
            pc_grouped[pc_layout.first].push_back(pc_layout.second);
        for (auto &pc_layout: pc_grouped) {
            sort(pc_layout.second.begin(), pc_layout.second.end());
            layout_stream << pc_layout.first.func << ' ' << pc_layout.first.inst << ' '
                          << pc_layout.second.size() << '\n';
            for (const auto &layout: pc_layout.second)
                layout_stream << get<0>(layout) << ' ' << get<1>(layout) << ' ' << get<2>(layout) << '\n';
        }
    }

    vector<tuple<PC, size_t, size_t>> all_fixed_mallocs;
    multimap<PC, tuple<size_t, size_t, size_t>> all_pcs_layout;
    ofstream layout_stream;
};

class FSRankStat {
public:
    explicit FSRankStat(const string &path) : stats_stream(path) {}

    void emplace(size_t fs, int mloc) {
        fs_mloc_ordering.emplace(fs, mloc);
    }

    void print() {
        for (auto it = fs_mloc_ordering.rbegin(); it != fs_mloc_ordering.rend(); it++)
            stats_stream << '#' << it->first << '@' << it->second << '\n';
    }

private:
    map<size_t, int> fs_mloc_ordering;
    ofstream stats_stream;
};

#endif //POSTPROCESS_STATS_H
