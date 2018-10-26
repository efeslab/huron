//
// Created by yifanz on 7/29/18.
//

#include <cstdint>
#include <map>
#include <stack>
#include <algorithm>
#include <iostream>

#include "Repair.h"

using namespace std;

class Layout {
public:
    Layout(const std::vector<std::tuple<Segment, PC, size_t>> &api_input, PC pc,
           int target_thread_count,
           const AnalysisResult *analysis) :
            target_thread_count(target_thread_count) {
        bool has_analysis = analysis && analysis->get_size_by_pc(pc, m_single_size);
        this->analysis = has_analysis ? analysis : nullptr;
        for (const auto &p: api_input)
            insert(get<1>(p), get<2>(p), get<0>(p));
        compute();
    }

private:
    void insert(const PC &pc, size_t tid, const Segment &range) {
        Segment analysis_range = range;
        auto key = make_pair(pc, tid);
        if (this->analysis && this->analysis->get_seg_by_pc(pc, analysis_range)) {
            size_t n = range.start / m_single_size;
            Segment offset_range = analysis_range.shift_by(n * m_single_size, true);
            this->access_relation[key].push_back(offset_range);
        } else
            this->access_relation[key].push_back(range);
        this->range_max = max(this->range_max, range.end);
    }

    void compute() {
        for (auto &p : this->access_relation) {
            sort(p.second.begin(), p.second.end());
            p.second = Segment::merge_neighbors(p.second);
        }
        // reclaim_unused_ranges();
        if (this->target_thread_count != -1) {
            if (is_linear()) {
                cout << "The segments are linear" << endl;
                extra_polate(this->target_thread_count);
            } else cout << "The segments are not linear" << endl;
        }
        map<Segment, size_t> thread_affinity = get_thread_affinity();
        merge_segments(thread_affinity);
        map<size_t, vector<Segment>> thread_grouped;
        for (const auto &p: thread_affinity)
            thread_grouped[p.second].push_back(p.first);
        for (auto &p: thread_grouped)
            sort(p.second.begin(), p.second.end());
        remappings = calc_remapping(thread_grouped);
        remap(remappings);
    }

    bool is_linear() {
        vector<PC> thread_group, main_group;
        map<uint32_t, bool> thread_maps;
        for (auto &iter : this->access_relation) {
            auto pc = iter.first.first;
            auto thread_id = iter.first.second;
            thread_maps[thread_id] = true;
            if (thread_id == 0) {
                //all in the same list
                main_group.push_back(pc);
            } else {
                //all in the different list
                if (find(thread_group.begin(), thread_group.end(), pc) == thread_group.end())thread_group.push_back(pc);
            }
        }
        size_t num_threads = thread_maps.size();
        //handle main group
        for (auto &i : main_group) {
            vector<Segment> ranges = this->access_relation[make_pair(i, 0)];
            size_t diff;
            for (size_t j = 0; j < ranges.size() - 1; j++) {
                Segment &s1 = ranges[j];
                Segment &s2 = ranges[j + 1];
                if ((s1.end - s1.start) != (s2.end - s2.start)) {
                    return false;
                }
                if (j == 0) {
                    diff = s2.end - s1.end;
                } else if (diff != s2.end - s1.end) {
                    return false;
                }
            }
        }
        //handle thread group
        for (auto &i : thread_group) {
            size_t diff;
            for (uint32_t j = 1; j < num_threads - 1; j++) {
                vector<Segment> range1 = this->access_relation[make_pair(i, j)];
                vector<Segment> range2 = this->access_relation[make_pair(i, j + 1)];
                if (range1.size() != 1)return false;
                if (range2.size() != 1)return false;
                Segment &s1 = range1[0];
                Segment &s2 = range2[0];
                //cout<<s1.start<<" "<<s1.end<<" "<<s2.start<<" "<<s2.end<<endl;
                if ((s1.end - s1.start) != (s2.end - s2.start)) {
                    return false;
                }
                if (j == 1) {
                    diff = s2.end - s1.end;
                } else if (diff != s2.end - s1.end) {
                    return false;
                }
            }
        }
        return true;
    }

    void extra_polate(int target_thread_count) {
        if (target_thread_count == -1)return;
        vector<PC> thread_group, main_group;
        map<uint32_t, bool> thread_maps;
        for (auto &iter : this->access_relation) {
            auto pc = iter.first.first;
            auto thread_id = iter.first.second;
            thread_maps[thread_id] = true;
            if (thread_id == 0) {
                //all in the same list
                main_group.push_back(pc);
            } else {
                //all in the different list
                if (find(thread_group.begin(), thread_group.end(), pc) == thread_group.end())thread_group.push_back(pc);
            }
        }
        int num_threads = static_cast<int>(thread_maps.size());
        if (num_threads - 1 >= target_thread_count)
            return;
        int num_extra_threads = target_thread_count - num_threads + 1;
        //handle main group
        for (auto &i : main_group) {
            vector<Segment> &ranges = this->access_relation[make_pair(i, 0)];
            size_t diff;
            size_t last_start, last_end;
            assert(ranges.size() > 1);
            diff = ranges[1].end - ranges[0].end;
            last_start = ranges[ranges.size() - 1].start;
            last_end = ranges[ranges.size() - 1].end;
            for (int j = 0; j < num_extra_threads; j++) {
                last_start += diff;
                last_end += diff;
                ranges.emplace_back(last_start, last_end);
            }
        }
        //handle thread group
        for (auto &i : thread_group) {
            size_t diff, last_start, last_end;
            vector<Segment> range1 = this->access_relation[make_pair(i, 1)];
            vector<Segment> range2 = this->access_relation[make_pair(i, 2)];
            diff = range2[0].start - range1[0].start;
            vector<Segment> range_last = this->access_relation[make_pair(i, num_threads - 1)];
            last_start = range_last[0].start;
            last_end = range_last[0].end;
            for (size_t j = 0; j < static_cast<size_t>(num_extra_threads); j++) {
                last_start += diff;
                last_end += diff;
                insert(i, j + num_threads, Segment(last_start, last_end));
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
                size_t thread = p.first.second;
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
                if (gap_usage == 1U << p.first.second || gap_usage == 0) {
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
        this->after_mapped_size = offset;
        return remappings;
    }

    void remap(const map<size_t, Segment> &remappings) {
        for (const auto &p: this->access_relation) {
            const PC &pc = p.first.first;
            size_t thread = p.first.second;
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

    friend class RepairPass;

    map<pair<PC, size_t>, vector<Segment>> access_relation;
    multimap<PC, tuple<size_t, size_t, size_t>> remapping_lines;
    map<size_t, Segment> remappings;
    size_t after_mapped_size, range_max;
    int target_thread_count;
    const AnalysisResult *analysis;
    size_t m_single_size;
};

const char *RepairPass::optionals = "[target_thread_count] [analysis_file]";
const size_t RepairPass::n_opt = 2;

RepairPass::RepairPass(const string &in, const vector<string> &rest) {
    assert(rest.size() <= n_opt);
    target_thread_count = (rest.empty()) ? -1 : stoi(rest[0]);
    ifstream is(in);
    read_from_file(is);
    if (rest.size() > 1) {
        ifstream anls(rest[1]);
        analysis.read_from_file(anls);
    }
}

RepairPass::RepairPass(DetectPass::ApiT &&in, const vector<string> &rest) {
    assert(rest.size() <= n_opt);
    target_thread_count = (rest.empty()) ? -1 : stoi(rest[0]);
    input = move(in);
    if (rest.size() > 1) {
        ifstream anls(rest[1]);
        analysis.read_from_file(anls);
    }
}

void RepairPass::compute() {
    for (const auto &p: input) {
        AnalysisResult *ap = analysis.empty() ? nullptr : &analysis;
        Layout layout(p.accesses, p.pc, target_thread_count, ap);
        const auto &result = layout.remapping_lines;
        all_pcs_layout.insert(result.begin(), result.end());
        size_t new_size = layout.after_mapped_size;
        if (new_size != p.size)
            all_fixed_mallocs.emplace_back(p.pc, p.size, new_size, layout.remappings);
    }
}

void RepairPass::print_result(const string &path) {
    ofstream layout_stream(path);
    print_malloc(layout_stream);
    print_layout(layout_stream);
}

void RepairPass::read_from_file(ifstream &is) {
    if (is.fail())
        throw std::invalid_argument("Can't open file\n");
    size_t n;
    is >> n;
    for (size_t i = 0; i < n; i++) {
        MallocOutput mo;
        is >> mo;
        input.emplace_back(mo);
    }
}

void RepairPass::print_malloc(ofstream &layout_stream) {
    layout_stream << all_fixed_mallocs.size() << '\n';
    for (const auto &m: all_fixed_mallocs) {
        if (m.pc == PC::null())
            layout_stream << "-1 -1 0 " << m.newSize << " 0\n";
        else {
            layout_stream << m.pc.func << ' ' << m.pc.inst << ' '
                          << m.origSize << ' ' << m.newSize << ' '
                          << m.translations.size() << '\n';
            for (size_t s: m.translations)
                layout_stream << s << ' ';
            layout_stream << '\n';
        }
    }
}

void RepairPass::print_layout(ofstream &layout_stream) {
    typedef tuple<size_t, size_t, size_t> LineT;
    map<PC, vector<LineT>> pc_grouped;
    for (const auto &pc_layout: all_pcs_layout)
        pc_grouped[pc_layout.first].push_back(pc_layout.second);
    for (auto &pc_layout: pc_grouped) {
        sort(pc_layout.second.begin(), pc_layout.second.end());
        layout_stream << pc_layout.first.func << ' ' << pc_layout.first.inst << ' '
                      << pc_layout.second.size() << '\n';
        for (const auto &layout: pc_layout.second)
            layout_stream << get<0>(layout) << ' ' << get<1>(layout) << ' '
                          << get<2>(layout) << '\n';
    }
}

void AnalysisResult::read_from_file(std::ifstream &is) {
    PC mpc;
    size_t m_single_size, n;
    while (is >> mpc >> m_single_size >> n) {
        this->malloc_single_size.emplace(mpc, m_single_size);
        for (size_t i = 0; i < n; i++) {
            PC ipc;
            Segment seg;
            is >> ipc >> seg;
            this->pc_replace.emplace(ipc, seg);
        }
    }
}

bool AnalysisResult::get_size_by_pc(PC pc, size_t &size) const {
    auto it = this->malloc_single_size.find(pc);
    if (it == this->malloc_single_size.end())
        return false;
    size = it->second;
    return true;
}

bool AnalysisResult::get_seg_by_pc(PC pc, Segment &seg) const {
    auto it = this->pc_replace.find(pc);
    if (it == this->pc_replace.end())
        return false;
    seg = it->second;
    return true;
}

bool AnalysisResult::empty() const {
    return this->pc_replace.empty();
}

FixedMalloc::FixedMalloc(PC pc, size_t origSize, size_t newSize, const std::map<size_t, Segment> &remap):
        pc(pc), origSize(origSize), newSize(newSize) {
    translations.resize(origSize);
    for (const auto &p: remap) {
        for (size_t i = 0; i < p.second.end - p.second.start; i++)
            translations[p.first + i] = p.second.start + i;
    }
}
