#include <algorithm>
#include <iostream>
#include <fstream>
#include <unordered_map>
#include <set>
#include <map>
#include <stack>
#include "Utils.h"

using namespace std;

struct RW {
    uint32_t r, w;

    explicit RW(uint32_t _r = 0, uint32_t _w = 0) : r(_r), w(_w) {}

    RW operator+(const RW &rhs) {
        return RW(r + rhs.r, w + rhs.w);
    }

    RW &operator+=(const RW &rhs) {
        r += rhs.r;
        w += rhs.w;
        return *this;
    }

    friend ostream &operator<<(ostream &os, const RW &rw) {
        os << '(' << rw.r << ", " << rw.w << ')';
        return os;
    }
};

struct PC {
    uint16_t func, inst;

    PC(uint16_t f, uint16_t i) : func(f), inst(i) {}

    bool operator==(const PC &rhs) const {
        return func == rhs.func && inst == rhs.inst;
    }

    bool operator<(const PC &rhs) const {
        if (func < rhs.func)
            return true;
        if (func > rhs.func)
            return false;
        return inst < rhs.inst;
    }

    friend ostream &operator<<(ostream &os, const PC &pc) {
        os << '(' << pc.func << ", " << pc.inst << ')';
        return os;
    }
};

namespace std {
    template<>
    struct hash<PC> {
        std::size_t operator()(PC const &pc) const {
            std::size_t res = 0;
            hash_combine(res, pc.func);
            hash_combine(res, pc.inst);
            return res;
        }
    };
}

struct Segment {
    size_t start, end;

    Segment(size_t _start = 0, size_t _end = 0) : start(_start), end(_end) {}

    bool operator==(const Segment &rhs) const {
        return start == rhs.start && end == rhs.end;
    }

    bool operator<(const Segment &rhs) const {
        return start < rhs.start || (start == rhs.start && end < rhs.end);
    }

    bool overlap(const Segment &rhs) const {
        return (start < rhs.end && end > rhs.start) || (rhs.start < end && rhs.end > start);
    }

    friend ostream &operator<<(ostream &os, const Segment &seg) {
        os << "(" << seg.start << ", " << seg.end << ")";
        return os;
    }

    Segment subtract_by(size_t sz) const {
        return {start - sz, end - sz};
    }

    static vector<Segment> merge_neighbors(const vector<Segment> &segs) {
        if (segs.empty())
            return vector<Segment>();
        vector<Segment> ret;
        Segment last = segs[0];
        for (size_t i = 1; i < segs.size(); i++) {
            if (last.end != segs[i].start) {
                ret.push_back(last);
                last = segs[i];
            } else
                last.end = segs[i].end;
        }
        ret.push_back(last);
        return ret;
    }
};

namespace std {
    template<>
    struct hash<Segment> {
        std::size_t operator()(Segment const &seg) const {
            std::size_t res = 0;
            hash_combine(res, seg.start);
            hash_combine(res, seg.end);
            return res;
        }
    };
}

struct Record {
    size_t addr;
    int m_id, m_offset;
    uint16_t thread, size;
    PC pc;
    RW rw;

    Record() : addr(0), m_id(0), m_offset(0), thread(0), size(0), pc(0, 0), rw(0, 0) {}

    friend istream &operator>>(istream &is, Record &rec) {
        static CSVParser csv(9);
        static string line;
        getline(is, line);
        if (line.empty())
            return is;
        const auto &fields = csv.read_csv_line(line);
        rec.thread = to_unsigned<uint16_t>(fields[0]);
        rec.addr = to_address(fields[1]);
        rec.m_id = to_signed<int>(fields[2]);
        rec.m_offset = to_signed<int>(fields[3]);
        rec.pc.func = to_unsigned<uint16_t>(fields[4]);
        rec.pc.inst = to_unsigned<uint16_t>(fields[5]);
        rec.size = to_unsigned<uint16_t>(fields[6]);
        rec.rw.r = to_unsigned<uint32_t>(fields[7]);
        rec.rw.w = to_unsigned<uint32_t>(fields[8]);

        return is;
    }
};

struct MallocInfo {
    size_t id, start, size;
    PC pc;

    MallocInfo() : id(0), start(0), size(0), pc(0, 0) {}

    friend istream &operator>>(istream &is, MallocInfo &mal) {
        static CSVParser csv(5);
        static string line;
        getline(is, line);
        if (line.empty())
            return is;
        const auto &fields = csv.read_csv_line(line);
        mal.id = to_unsigned<size_t>(fields[0]);
        mal.start = to_unsigned<size_t>(fields[1]);
        mal.size = to_unsigned<size_t>(fields[2]);
        mal.pc.func = to_unsigned<uint16_t>(fields[3]);
        mal.pc.inst = to_unsigned<uint16_t>(fields[4]);
        return is;
    }

    bool operator<(const MallocInfo &rhs) const {
        return id < rhs.id;
    }
};

class AddrRecord {
public:
    friend class MallocStorageT;

    AddrRecord(Segment _range, int m_id, size_t m_start, const vector<Record> &records) :
            range(_range), malloc_start(m_start), malloc_id(m_id) {
        for (const auto &rec: records) {
            thread_rw[rec.thread] += rec.rw;
            pc_rw[rec.pc] += rec.rw;
            pc_threads.emplace(rec.pc, rec.thread);
        }
    }

    pair<size_t, size_t> cachelines() const {
        size_t addr_start = malloc_start + range.start, addr_end_incl = malloc_start + range.end - 1;
        size_t start_cl = addr_start >> CACHELINE_BIT, end_cl = addr_end_incl >> CACHELINE_BIT;
        return make_pair(start_cl, end_cl);
    }

    friend ostream &operator<<(ostream &os, const AddrRecord &rec) {
        auto p = rec.cachelines();
        size_t addr_start = rec.malloc_start + rec.range.start, addr_end = rec.malloc_start + rec.range.end;
        size_t l = addr_start - (p.first << CACHELINE_BIT), r = addr_end - (p.second << CACHELINE_BIT);
        os << hex;
        if (p.first != p.second)
            os << "(" << p.first << "+0x" << l << ", " << p.second << "+0x" << r << ");";
        else
            os << "(" << "0x" << l << ", " << "0x" << r << ");";
        os << "m(" << "0x" << rec.range.start << ", 0x" << rec.range.end << ')' << ": ";
        os << dec;
        print_map(os, rec.thread_rw);
        os << "  ";
        print_map(os, rec.pc_rw);
        return os;
    }

    vector<bool> get_thread_ids() const {
        uint32_t max_th = 0;
        for (const auto &p: thread_rw)
            max_th = max(max_th, p.first);
        vector<bool> threads(max_th + 1);
        for (const auto &p: thread_rw)
            threads[p.first] = true;
        return threads;
    }

    static RW get_total_rw(const vector<AddrRecord> &records) {
        RW ret;
        for (const auto &rec: records)
            for (const auto &p: rec.thread_rw)
                ret += p.second;
        return ret;
    }

    static RW get_total_rw_excluding(const vector<AddrRecord> &records,
                                     const vector<bool> &exclude_threads) {
        RW ret;
        for (const auto &rec: records)
            for (const auto &p: rec.thread_rw)
                if (exclude_threads.size() <= p.first || !exclude_threads[p.first])
                    ret += p.second;
        return ret;
    }

    bool operator==(const AddrRecord &rhs) const {
        return range == rhs.range;
    }

    bool operator<(const AddrRecord &rhs) const {
        return range < rhs.range;
    }

    const Segment &get_segment() const {
        return range;
    };

private:
    unordered_map<uint32_t, RW> thread_rw;
    unordered_map<PC, RW> pc_rw;
    unordered_multimap<PC, uint32_t> pc_threads;
    Segment range;
    size_t malloc_start;
    int malloc_id;
};

class Graph {
private:
    struct GraphGroup {
        vector<bool> threads;
        vector<AddrRecord> records;

        GraphGroup() = default;

        explicit GraphGroup(pair<vector<bool>, vector<AddrRecord>> &&pair) :
                threads(std::move(pair.first)), records(std::move(pair.second)) {}

        static size_t rhs_rw_suffer_from_lhs(const GraphGroup &lhs, const GraphGroup &rhs) {
            RW lhs_rw = AddrRecord::get_total_rw(lhs.records);
            RW rhs_minus_lhs_rw = AddrRecord::get_total_rw_excluding(rhs.records, lhs.threads);
            return min(lhs_rw.w, rhs_minus_lhs_rw.r + rhs_minus_lhs_rw.w);
        }

        friend ostream &operator<<(ostream &os, const GraphGroup &gg) {
            print_bv_as_set(os, gg.threads);
            os << '\n';
            for (const auto &g0: gg.records)
                os << g0 << '\n';
            return os;
        }
    };

public:
    explicit Graph(pair<size_t, vector<AddrRecord>> &&_records) :
            clid(_records.first) {
        records = move(_records.second);
        sort(records.begin(), records.end());
        estm_fs = estm_false_sharing();
    }

    vector<GraphGroup> thread_groups(const vector<AddrRecord> &v) const {
        unordered_map<vector<bool>, vector<AddrRecord>> grouping_map;
        vector<GraphGroup> groups;
        for (const auto &rec: v)
            grouping_map[rec.get_thread_ids()].push_back(rec);
        for (const auto &p: grouping_map)
            groups.emplace_back(p);
        return groups;
    }

    size_t get_n_false_sharing() const {
        return estm_fs;
    }

    bool operator<(const Graph &rhs) const {
        return clid < rhs.clid;
    }

    friend ostream &operator<<(ostream &os, const Graph &g) {
        os << ">>>0x" << hex << g.clid << dec << '(' << g.estm_fs << ")<<<\n";
        for (const auto &rec: g.records)
            os << rec << '\n';
        os << '\n';
        return os;
    }

private:
    size_t estm_false_sharing() const {
        size_t total_rw = 0;
        auto groups = thread_groups(records);
        for (size_t i = 0; i < groups.size(); i++) {
            size_t max_rw = 0;
            for (size_t j = i + 1; j < groups.size(); j++) {
                size_t ij_rw = GraphGroup::rhs_rw_suffer_from_lhs(groups[i], groups[j]);
                size_t ji_rw = GraphGroup::rhs_rw_suffer_from_lhs(groups[j], groups[i]);
                max_rw = max(max_rw, max(ij_rw, ji_rw));
            }
            total_rw += max_rw;
        }
        return total_rw;
    }

    size_t clid, estm_fs;
    vector<AddrRecord> records;
};


class MallocStorageT {
public:
    MallocStorageT(const MallocStorageT &) = delete;

    MallocStorageT(MallocStorageT &&) = default;

    MallocStorageT() : minfo(), malloc_fs(0), m_id(0) {}

    explicit MallocStorageT(
            int _m_id, const MallocInfo &_m,
            const unordered_map<Segment, vector<Record>> &bucket,
            size_t graph_threshold
    ) :
            minfo(_m), malloc_fs(0), m_id(_m_id) {
        size_t m_start = _m.start;
        find_overlap(_m_id, m_start, bucket);
        for (const auto &p: bucket) {
            Segment seg = p.first.subtract_by(m_start);
            records.emplace_back(seg, _m_id, m_start, p.second);
        }
        calc_graphs(graph_threshold);
        if (m_id != -1 && !graphs.empty())
            get_fixed_layout();
    }

    void calc_graphs(size_t threshold) {
        map<size_t, vector<AddrRecord>> cachelines;
        for (const auto &rec: records) {
            auto cls = rec.cachelines();
            assert(cls.first <= cls.second);
            for (size_t i = cls.first; i <= cls.second; i++)
                cachelines[i].push_back(rec);
        }
        for (auto &p: cachelines)
            sort(p.second.begin(), p.second.end());
        graphs.reserve(cachelines.size());
        for (const auto &pair: cachelines)
            graphs.emplace_back(pair);
        sort(graphs.begin(), graphs.end());
        malloc_fs = accumulate(graphs.begin(), graphs.end(), 0ul,
                               [](size_t rhs, const Graph &lhs) { return rhs + lhs.get_n_false_sharing(); });
        remove_erase_count_if(graphs, [threshold](const Graph &g) {
            return g.get_n_false_sharing() < threshold;
        });
    }

    bool empty() {
        return graphs.empty();
    }

    size_t get_n_false_sharing() const {
        return malloc_fs;
    }

    friend ostream &operator<<(ostream &os, const MallocStorageT &mst) {
        os << "=================" << mst.m_id << "(" << mst.malloc_fs << ")================\n";
        for (const Graph &g: mst.graphs)
            os << g;
        return os;
    }

    multimap<PC, tuple<size_t, size_t, size_t>> get_pc_grouped_layout() const {
        multimap<PC, tuple<size_t, size_t, size_t>> ret;
        for (const auto &p: access_relation) {
            const PC &pc = p.first.first;
            uint32_t thread = p.first.second;
            for (const auto &seg: p.second) {
                auto it = remappings.upper_bound(seg.start);
                assert(it != remappings.begin());
                it--;
                size_t size = it->second.end - it->second.start;
                assert(seg.start >= it->first && seg.end <= it->first + size);
                size_t mapped = seg.start - it->first + it->second.start;
                ret.emplace(pc, make_tuple(thread, seg.start, mapped));
            }
        }
        return ret;
    }

    void get_fixed_layout() {
        for (const auto &rec: records) {
            Segment rec_range = rec.get_segment();
            for (const auto &p: rec.pc_threads)
                this->access_relation[p].push_back(rec_range);
        }
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
        auto it = thread_affinity.begin();
        stack<pair<Segment, size_t>> merging_stack;
        merging_stack.push(*it);
        it++;
        for (; it != thread_affinity.end(); it++) {
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
        thread_affinity.clear();
        while (!merging_stack.empty()) {
            thread_affinity.insert(merging_stack.top());
            merging_stack.pop();
        }
        map<size_t, vector<Segment>> thread_grouped;
        for (const auto &p: thread_affinity)
            thread_grouped[p.second].push_back(p.first);
        for (auto &p: thread_grouped)
            sort(p.second.begin(), p.second.end());
        size_t offset = 0;
        for (const auto &p : thread_grouped) {
            if (offset != 0)
                offset += 1 << CACHELINE_BIT;
            for (const auto &seg: p.second) {
                Segment map_to(offset, offset + seg.end - seg.start);
                this->remappings[seg.start] = map_to;
                offset = map_to.end;
            }
        }
        this->after_mapped = offset;
    }

    void print_fixed_malloc(ostream &os) const {
        os << minfo.pc.func << ' ' << minfo.pc.inst << ' ' << minfo.size
           << ' ' << after_mapped << '\n';
    }

private:
    void find_overlap(int m_id, size_t m_start, const unordered_map<Segment, vector<Record>> &bucket) {
        const Segment *prev = nullptr;
        for (const auto &it : bucket) {
            if (prev && prev->overlap(it.first))
                cerr << "Warning: offset range " << prev->subtract_by(m_start) << " overlaps with "
                     << it.first.subtract_by(m_start) << " in malloc " << m_id << '\n';
            prev = &it.first;
        }
    }

    vector<AddrRecord> records;
    vector<Graph> graphs;
    map<size_t, Segment> remappings;
    map<pair<PC, uint32_t>, vector<Segment>> access_relation;
    MallocInfo minfo;
    size_t malloc_fs, after_mapped;
    int m_id;
};

map<int, MallocStorageT> compute_from_log(ifstream &logf, ifstream &mf, size_t graph_threshold) {
    map<int, unordered_map<Segment, vector<Record>>> bins;
    map<int, MallocInfo> mallocs;
    map<int, MallocStorageT> ret;
    Record next_r;
    MallocInfo next_m;
    while (mf >> next_m)
        mallocs[next_m.id] = next_m;
    size_t i = 0;
    while (logf >> next_r) {
        if (!(i++ % 10000))
            cout << "line of log read: " << i - 1 << endl;
        auto key = Segment(next_r.addr, next_r.addr + next_r.size);
        bins[next_r.m_id][key].push_back(next_r);
    }
    cout << "line of log read: " << i - 1 << endl;
    i = 0;
    for (const auto &p: bins) {
        if (!(i++ % 1000))
            cout << "# of mallocs processed: " << i - 1 << '/' << bins.size() << endl;
        MallocStorageT malloc_t(p.first, mallocs[p.first], p.second, graph_threshold);
        if (!malloc_t.empty())
            ret.emplace(p.first, move(malloc_t));
    }
    cout << "# of mallocs processed: " << bins.size() << '/' << bins.size() << endl;
    return ret;
}

void print_pc_trans_table(ostream &os,
                          const multimap<PC, tuple<size_t, size_t, size_t>> &all_pcs_layout) {
    typedef tuple<size_t, size_t, size_t> LayoutT;
    map<PC, vector<LayoutT>> pc_grouped;
    for (const auto &pc_layout: all_pcs_layout)
        pc_grouped[pc_layout.first].push_back(pc_layout.second);
    for (auto &pc_layout: pc_grouped) {
        sort(pc_layout.second.begin(), pc_layout.second.end());
        os << pc_layout.first.func << ' ' << pc_layout.first.inst << ' '
           << pc_layout.second.size() << '\n';
        for (const auto &layout: pc_layout.second)
            os << get<0>(layout) << ' ' << get<1>(layout) << ' ' << get<2>(layout) << '\n';
    }
}

int main(int argc, char *argv[]) {
    if (argc < 3 || argc > 5) {
        cerr << "Usage: " << argv[0] << " logfile output [threshold] [mallocfile]" << endl;
        return 1;
    }
    string log_path = argv[1], out_path = argv[2];
    string malloc_path = argc == 5 ? argv[4] : "mallocRuntimeIDs.txt";
    ifstream log_file(log_path), malloc_file(malloc_path);
    if (log_file.fail() || malloc_file.fail()) {
        cerr << "Can't open file\n";
        return 1;
    }
    std::ios_base::sync_with_stdio(false);
    size_t threshold = (argc >= 4) ? stoul(argv[3]) : 100;

    auto groups = compute_from_log(log_file, malloc_file, threshold);
    ofstream graphs_stream(insert_suffix(log_path, "_summary"));
    ofstream stats_stream(insert_suffix(log_path, "_fs_malloc"));
    ofstream layout_stream(out_path);

    size_t n_mallocs = 0;
    map<size_t, size_t> fs_cl_ordering;
    multimap<PC, tuple<size_t, size_t, size_t>> all_pcs_layout;

    layout_stream << groups.size() << '\n';
    for (const auto &pair: groups) {
        n_mallocs++;
        fs_cl_ordering.emplace(pair.second.get_n_false_sharing(), pair.first);
        graphs_stream << pair.second;
        auto layout = pair.second.get_pc_grouped_layout();
        all_pcs_layout.insert(layout.begin(), layout.end());
        pair.second.print_fixed_malloc(layout_stream);
    }

    print_pc_trans_table(layout_stream, all_pcs_layout);

    for (auto it = fs_cl_ordering.rbegin(); it != fs_cl_ordering.rend(); it++)
        stats_stream << '#' << it->first << '@' << it->second << '\n';
}
