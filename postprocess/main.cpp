#include <algorithm>
#include <iostream>
#include <fstream>
#include <unordered_map>
#include <unordered_set>
#include <set>
#include <map>
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

    Segment(size_t _start, size_t _end) : start(_start), end(_end) {}

    bool operator==(const Segment &rhs) const {
        return start == rhs.start && end == rhs.end;
    }

    bool operator<(const Segment &rhs) const {
        return start < rhs.start;
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

class AddrRecord {
public:
    AddrRecord(Segment _range, int m_id, size_t m_start, const vector<Record> &records) :
            range(_range), malloc_start(m_start), malloc_id(m_id) {
        for (const auto &rec: records) {
            thread_rw[rec.thread] += rec.rw;
            pc_rw[rec.pc] += rec.rw;
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

    bool operator<(const AddrRecord &rhs) const {
        return range < rhs.range;
    }

    void emit_api_output(ostream &os) const {
        os << range.start << ' ' << (range.end - range.start) << ' '
           << thread_rw.size() << ' ';
        for (const auto &p2: thread_rw)
            os << p2.first << ' ' << p2.second.r << ' ' << p2.second.w << ' ';
        os << '\n';
    }

private:
    unordered_map<uint32_t, RW> thread_rw;
    unordered_map<PC, RW> pc_rw;
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

    MallocStorageT() : malloc_fs(0), n_records(0), max_malloc_offset(0), m_id(0) {}

    explicit MallocStorageT(int _m_id, size_t _m_start, const unordered_map<Segment, vector<Record>> &bucket,
                            size_t graph_threshold) :
            malloc_fs(0), n_records(0), max_malloc_offset(0), m_id(_m_id) {
        find_overlap(_m_id, _m_start, bucket);
        for (const auto &p: bucket) {
            Segment seg = p.first.subtract_by(_m_start);
            records.emplace_back(seg, _m_id, _m_start, p.second);
            for (const auto &rec: p.second) {
                if (rec.m_offset != -1)
                    max_malloc_offset = max(max_malloc_offset, size_t(rec.m_offset));
                all_pcs.insert(rec.pc);
            }
        }
        for (const auto &rec: records) {
            auto cls = rec.cachelines();
            assert(cls.first <= cls.second);
            for (size_t i = cls.first; i <= cls.second; i++)
                cachelines[i].push_back(rec);
        }
        for (auto &p: cachelines)
            sort(p.second.begin(), p.second.end());
        calc_graphs(graph_threshold);
    }

    void calc_graphs(size_t threshold) {
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

    void emit_api_output(ostream &os) const {
        os << m_id << '\n';
        os << all_pcs.size() << '\n';
        for (const PC &pc: all_pcs)
            os << pc.func << ' ' << pc.inst << '\n';
        os << max_malloc_offset << '\n';
        os << n_records << '\n';
        for (const auto &rec: records)
            rec.emit_api_output(os);
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

    map<size_t, vector<AddrRecord>> cachelines;
    vector<AddrRecord> records;
    vector<Graph> graphs;
    set<PC> all_pcs;
    size_t malloc_fs, n_records, max_malloc_offset;
    int m_id;
};

map<int, MallocStorageT> compute_from_log(ifstream &file, size_t graph_threshold) {
    map<int, unordered_map<Segment, vector<Record>>> bins;
    map<int, size_t> malloc_start;
    map<int, MallocStorageT> ret;
    Record next;
    size_t i = 0;
    while (file >> next) {
        if (!(i++ % 10000))
            cout << "line of log read: " << i - 1 << endl;
        auto key = Segment(next.addr, next.addr + next.size);
        bins[next.m_id][key].push_back(next);
        malloc_start.emplace(next.m_id, next.addr - next.m_offset);
    }
    i = 0;
    for (const auto &p: bins) {
        if (!(i++ % 1000))
            cout << "# of mallocs processed: " << i - 1 << '/' << bins.size() << endl;
        MallocStorageT malloc_t(p.first, malloc_start[p.first], p.second, graph_threshold);
        if (!malloc_t.empty())
            ret.emplace(p.first, move(malloc_t));
    }
    return ret;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        cerr << "Usage: " << argv[0] << " LOGFILE" << endl;
        return 1;
    }
    string path = argv[1];
    ifstream file(path);
    if (file.fail())
        return 1;
    std::ios_base::sync_with_stdio(false);
    size_t threshold = (argc == 3) ? stoul(argv[2]) : 100;

    auto groups = compute_from_log(file, threshold);
    ofstream graphs_stream(insert_suffix(path, "_summary"));
    ofstream api_stream(insert_suffix(path, "_output"));
    ofstream stats_stream(insert_suffix(path, "_fs_malloc"));
    size_t n_mallocs = 0;
    map<size_t, size_t> fs_cl_ordering;

    api_stream << groups.size() << '\n';
    for (const auto &pair: groups) {
        n_mallocs++;
        fs_cl_ordering.emplace(pair.second.get_n_false_sharing(), pair.first);
        graphs_stream << pair.second;
        pair.second.emit_api_output(api_stream);
    }

    for (auto it = fs_cl_ordering.rbegin(); it != fs_cl_ordering.rend(); it++)
        stats_stream << '#' << it->first << '@' << it->second << '\n';
}
