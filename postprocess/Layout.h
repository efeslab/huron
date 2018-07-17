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
    Layout() : after_mapped(0), range_max(0) {}

    void insert(const PC &pc, uint32_t tid, const Segment &range) {
        this->access_relation[make_pair(pc, tid)].push_back(range);
        this->range_max = max(this->range_max, range.end);
    }

    void compute() {
        for (auto &p : this->access_relation) {
            sort(p.second.begin(), p.second.end());
            p.second = Segment::merge_neighbors(p.second);
        }
        // reclaim_unused_ranges();
        map<Segment, size_t> thread_affinity = get_thread_affinity();
        merge_segments(thread_affinity);
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
    void merge_segments(map<Segment, size_t> &arg) {
        auto it = arg.begin();
        vector<pair<Segment, size_t>> overlaps_stack;
        overlaps_stack.emplace_back(*it);
        it++;
        for (; it != arg.end(); it++) {
            auto &top = overlaps_stack.back();
            // If overlaps, extend the Segment at stack top to include the new one,
            // joining their thread affinity (extra false sharing may be introduced).
            // Otherwise, just push the new one.
            if (top.first.overlap(it->first)) {
                top.first.end = max(top.first.end, it->first.end);
                top.second |= it->second;
            } else
                overlaps_stack.emplace_back(*it);
        }
        arg.clear();
        if (overlaps_stack.empty())
            return;
        // Do a simplifying pass: if two neighboring segments
        // touches exactly and have the same affinity, merge them.
        vector<pair<Segment, size_t>> touches_stack;
        touches_stack.emplace_back(overlaps_stack.front());
        for (size_t i = 1; i < overlaps_stack.size(); i++) {
            auto &top = touches_stack.back(), &next = overlaps_stack[i];
            if (top.first.touch(next.first) && top.second == next.second)
                top.first.end = next.first.end;
            else
                touches_stack.emplace_back(next);
        }
        arg.insert(touches_stack.begin(), touches_stack.end());
    }

    map<Segment, size_t> get_thread_affinity() {
        map<Segment, size_t> thread_affinity;
        for (const auto &p: this->access_relation)
            for (const auto &seg: p.second) {
                uint32_t thread = p.first.second;
                assert(thread < sizeof(size_t) * 8);
                thread_affinity[seg] |= 1 << thread;
            }
        return thread_affinity;
    }

    void reclaim_unused_ranges() {
        map<Segment, size_t> thread_affinity = get_thread_affinity();
        auto *affinity_bitmap = new size_t[this->range_max]();
        for (const auto &p: thread_affinity)
            for (size_t i = p.first.start; i < p.first.end; i++)
                affinity_bitmap[i] |= p.second;
        for (auto &p: this->access_relation) {
            if (p.second.empty())
                continue;
            for (auto it = p.second.begin() + 1; it != p.second.end();) {
                const Segment &prev = *(it - 1), &next = *it;
                Segment gap(prev.end, next.start);
                size_t gap_usage = 0;
                for (size_t j = gap.start; j < gap.end; j++)
                    gap_usage |= affinity_bitmap[j];
                if (gap_usage == 1 << p.first.second || gap_usage == 0) {
                    it = p.second.erase(it - 1, it + 1);
                    it = p.second.emplace(it, prev.start, next.end);
                } else
                    ++it;
            }
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
            else if (!lines.empty())
                remapping_lines.emplace(pc, lines.front());
        }
    }

    map<pair<PC, uint32_t>, vector<Segment>> access_relation;
    multimap<PC, tuple<size_t, size_t, size_t>> remapping_lines;
    size_t after_mapped, range_max;
};

#endif //POSTPROCESS_LAYOUT_H
