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

    void compute(int target_thread_count=-1) {
        for (auto &p : this->access_relation) {
            sort(p.second.begin(), p.second.end());
            p.second = Segment::merge_neighbors(p.second);
        }
        // reclaim_unused_ranges();
        if(target_thread_count != -1)
        {
            if(is_linear())
            {
                cout<<"The segments are linear"<<endl;
                extra_polate(target_thread_count);
            }
            else cout<<"The segments are not linear"<<endl;
        }
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
    bool is_linear() {
        vector<PC> thread_group, main_group;
        map<uint32_t, bool> thread_maps;
        for(auto &iter : this->access_relation)
        {
            auto pc = iter.first.first;
            auto thread_id = iter.first.second;
            thread_maps[thread_id] = true;
            if(thread_id == 0) {
                //all in the same list
                main_group.push_back(pc);
            } else {
                //all in the different list
                if(find(thread_group.begin(), thread_group.end(), pc) == thread_group.end())thread_group.push_back(pc);
            }
        }
        uint32_t num_threads = thread_maps.size();
        //handle main group
        for(int i=0; i < main_group.size(); i++)
        {
            vector<Segment> ranges = this->access_relation[make_pair(main_group[i], 0)];
            uint32_t diff, size;
            for(int j=0; j < ranges.size() - 1; j++)
            {
                Segment &s1 = ranges[j];
                Segment &s2 = ranges[j+1];
                //cout<<s1.start<<" "<<s1.end<<" "<<s2.start<<" "<<s2.end<<endl;
                if((s1.end - s1.start) != (s2.end - s2.start))
                {
                    return false;
                }
                if(j==0)
                {
                    diff = s2.end - s1.end; 
                } else if (diff != s2.end - s1.end) {
                    return false;
                }
            }
        }
        //handle thread group
        for(int i=0; i < thread_group.size(); i++)
        {
            uint32_t diff, size;
            for(uint32_t j=1; j < num_threads - 1; j++)
            {
                vector<Segment> range1 = this->access_relation[make_pair(thread_group[i],j)];
                vector<Segment> range2 = this->access_relation[make_pair(thread_group[i],j+1)];
                if(range1.size()!=1)return false;
                if(range2.size()!=1)return false;
                Segment &s1 = range1[0];
                Segment &s2 = range2[0];
                //cout<<s1.start<<" "<<s1.end<<" "<<s2.start<<" "<<s2.end<<endl;
                if((s1.end - s1.start) != (s2.end - s2.start))
                {
                    return false;
                }
                if(j==1)
                {
                    diff = s2.end - s1.end; 
                } else if (diff != s2.end - s1.end) {
                    return false;
                }
            }
        }
        return true;
    }
    void extra_polate(int target_thread_count)
    {
        if(target_thread_count == -1)return;
        vector<PC> thread_group, main_group;
        map<uint32_t, bool> thread_maps;
        for(auto &iter : this->access_relation)
        {
            auto pc = iter.first.first;
            auto thread_id = iter.first.second;
            thread_maps[thread_id] = true;
            if(thread_id == 0) {
                //all in the same list
                main_group.push_back(pc);
            } else {
                //all in the different list
                if(find(thread_group.begin(), thread_group.end(), pc) == thread_group.end())thread_group.push_back(pc);
            }
        }
        uint32_t num_threads = thread_maps.size();
        if(num_threads - 1 >= target_thread_count)return;
        int num_extra_threads = target_thread_count - num_threads + 1;
        //handle main group
        for(int i=0; i < main_group.size(); i++)
        {
            vector<Segment> &ranges = this->access_relation[make_pair(main_group[i], 0)];
            uint32_t diff;
            size_t last_start, last_end;
            assert(ranges.size() > 1);
            diff = ranges[1].end - ranges[0].end;
            last_start = ranges[ranges.size()-1].start;
            last_end = ranges[ranges.size()-1].end;
            for(int j = 0; j < num_extra_threads; j++)
            {
                last_start += diff;
                last_end += diff;
                ranges.push_back(Segment(last_start, last_end));
            }
        }
        //handle thread group
        for(int i=0; i < thread_group.size(); i++)
        {
            uint32_t diff;
            size_t last_start, last_end;
            vector<Segment> range1 = this->access_relation[make_pair(thread_group[i],1)];
            vector<Segment> range2 = this->access_relation[make_pair(thread_group[i],2)];
            diff = range2[0].start - range1[0].start;
            vector<Segment> range_last = this->access_relation[make_pair(thread_group[i],num_threads-1)];
            last_start = range_last[0].start;
            last_end = range_last[0].end;
            for(uint32_t j=0; j < num_extra_threads; j++)
            {
                last_start+=diff;
                last_end += diff;
                insert(thread_group[i], j+num_threads, Segment(last_start, last_end));
            }
        }
    }
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
