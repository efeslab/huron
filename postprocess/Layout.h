//
// Created by yifanz on 7/10/18.
//

#ifndef POSTPROCESS_LAYOUT_H
#define POSTPROCESS_LAYOUT_H

#include <cstdint>
#include <map>
#include <stack>
#include <algorithm>
#include "Utils.h"

class Layout {
public:
    Layout() : after_mapped(0) {}

    void insert(const PC &pc, uint32_t tid, const Segment &range) {
        this->access_relation[make_pair(pc, tid)].push_back(range);
    }

    void compute() {
        for (auto &p : this->access_relation) {
            sort(p.second.begin(), p.second.end());
            p.second = Segment::merge_neighbors(p.second);
        }
        map<Segment, size_t> thread_affinity;
        for (const auto &p: this->access_relation)
            for (const auto &seg: p.second) {
                uint32_t thread = p.first.second;
                assert(thread < sizeof(size_t) * 8);
                thread_affinity[seg] |= 1 << thread;
            }
        merge_overlapping_segments(thread_affinity);
        map<size_t, vector<Segment>> thread_grouped;
        for (const auto &p: thread_affinity)
            thread_grouped[p.second].push_back(p.first);
        for (auto &p: thread_grouped)
            sort(p.second.begin(), p.second.end());
        map<size_t, Segment> remappings = calc_remapping(thread_grouped);
        remap(remappings);
    }

    multimap<PC, tuple<size_t, size_t, size_t>> get_remapping() {
        return remapping_lines;
    }

    size_t get_new_size() {
        return after_mapped;
    }

private:
    void merge_overlapping_segments(map<Segment, size_t> &arg) {
        auto it = arg.begin();
        stack<pair<Segment, size_t>> merging_stack;
        merging_stack.push(*it);
        it++;
        for (; it != arg.end(); it++) {
            auto &top = merging_stack.top();
            // if current interval is not overlapping with stack top,
            // push it to the stack
            // Otherwise update the ending time of top
            if (top.first.end <= it->first.start)
                merging_stack.push(*it);
            else {
                top.first.end = max(top.first.end, it->first.end);
                top.second |= it->second;
            }
        }
        arg.clear();
        while (!merging_stack.empty()) {
            arg.insert(merging_stack.top());
            merging_stack.pop();
        }
    }

    map<size_t, Segment> calc_remapping(const map<size_t, vector<Segment>> &thread_grouped) {
        map<size_t, Segment> remappings;
        size_t offset = 0;
        for (const auto &p : thread_grouped) {
            if (offset != 0)
                offset += 1 << CACHELINE_BIT;
            for (const auto &seg: p.second) {
                Segment map_to(offset, offset + seg.end - seg.start);
                remappings[seg.start] = map_to;
                offset = map_to.end;
            }
        }
        this->after_mapped = offset;
        return remappings;
    }

    void remap(const map<size_t, Segment> &remappings) {
        for (const auto &p: this->access_relation) {
            const PC &pc = p.first.first;
            uint32_t thread = p.first.second;
            AllEqual<long> ae;
            vector<tuple<size_t, size_t, size_t>> lines;
            for (const auto &seg: p.second) {
                auto it = remappings.upper_bound(seg.start);
                assert(it != remappings.begin());
                it--;
                size_t size = it->second.end - it->second.start;
                assert(seg.start >= it->first && seg.end <= it->first + size);
                long offset = it->second.start - it->first;
                ae.next(offset);
                lines.emplace_back(thread, seg.start, seg.start + offset);
            }
            if (!ae.is_all_equal())
                for (const auto &t: lines)
                    remapping_lines.emplace(pc, t);
            else if (!lines.empty() && ae.last_value() != 0)
                remapping_lines.emplace(pc, lines.front());
        }
    }
    map<pair<PC, uint32_t>, vector<Segment>> access_relation;
    multimap<PC, tuple<size_t, size_t, size_t>> remapping_lines;
    size_t after_mapped;
};

#endif //POSTPROCESS_LAYOUT_H
