//
// Created by yifanz on 7/29/18.
//

#include <algorithm>
#include <iostream>
#include <fstream>
#include <unordered_map>
#include <set>
#include <map>
#include <stack>
#include <numeric>
#include "Detect.h"

using namespace std;

struct Record {
    size_t addr;
    int m_id;
    uint16_t thread, size;
    PC pc;
    RW rw;

    Record() : addr(0), m_id(0), thread(0), size(0), pc(0, 0), rw(0, 0) {}

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
        rec.pc.func = to_unsigned<uint16_t>(fields[4]);
        rec.pc.inst = to_unsigned<uint16_t>(fields[5]);
        rec.size = to_unsigned<uint16_t>(fields[6]);
        rec.rw.r = to_unsigned<uint32_t>(fields[7]);
        rec.rw.w = to_unsigned<uint32_t>(fields[8]);

        return is;
    }
};

struct MallocInfo {
    size_t start, size;
    PC pc;
    int id;

    MallocInfo() : start(0), size(0), pc(0, 0), id(0) {}

    friend istream &operator>>(istream &is, MallocInfo &mal) {
        static CSVParser csv(5);
        static string line;
        getline(is, line);
        if (line.empty())
            return is;
        const auto &fields = csv.read_csv_line(line);
        mal.id = to_signed<int>(fields[0]);
        mal.start = to_unsigned<size_t>(fields[1]);
        mal.size = to_unsigned<size_t>(fields[2]);
        int func = to_signed<int>(fields[3]);
        if (func == -1)
            mal.pc = PC::null();
        else {
            mal.pc.func = to_unsigned<uint16_t>(fields[3]);
            mal.pc.inst = to_unsigned<uint16_t>(fields[4]);
        }
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
        {
            os << '{';
            bool need_comma = false;
            for (const auto &p2: rec.thread_rw) {
                if (need_comma)
                    os << ", ";
                os << p2.first << ": ";
                p2.second.dump(os);
                need_comma = true;
            }
            os << '}';
        }
        os << "  ";
        {
            os << '{';
            bool need_comma = false;
            for (const auto &p2: rec.pc_rw) {
                if (need_comma)
                    os << ", ";
                p2.first.dump(os);
                os << ": ";
                p2.second.dump(os);
                need_comma = true;
            }
            os << '}';
        }
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

private:
    unordered_map <uint32_t, RW> thread_rw;
    unordered_map <PC, RW> pc_rw;
    unordered_multimap <PC, uint32_t> pc_threads;
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
            os << '{';
            bool need_comma = false;
            for (size_t i = 0; i < gg.threads.size(); i++) {
                if (!gg.threads[i])
                    continue;
                if (need_comma)
                    os << ", ";
                os << i;
                need_comma = true;
            }
            os << '}';
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
        std::unordered_map<vector<bool>, vector<AddrRecord>> grouping_map;
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
        os << g.clid << "," << g.estm_fs << '\n';
        /*os << ">>>0x" << hex << g.clid << dec << '(' << g.estm_fs << ")<<<\n";
        for (const auto &rec: g.records)
            os << rec << '\n';
        os << '\n';*/
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
            const std::unordered_map<Segment, vector<Record>> &bucket,
            size_t graph_threshold) :
            minfo(_m), malloc_fs(0), m_id(_m_id) {
        size_t m_start = _m.start;
        find_overlap(_m_id, m_start, bucket);
        for (const auto &p: bucket) {
            Segment seg = p.first.shift_by(m_start, false);
            records.emplace_back(seg, _m_id, m_start, p.second);
        }
        calc_graphs(graph_threshold);
    }

    bool valid() {
        return !graphs.empty();
    }

    size_t get_n_false_sharing() const {
        return malloc_fs;
    }

    pair<PC, size_t> get_malloc_info() const {
        return make_pair(minfo.pc, minfo.size);
    }

    friend ostream &operator<<(ostream &os, const MallocStorageT &mst) {
        //os << "=================" << mst.m_id << "(" << mst.malloc_fs << ")================\n";
        for (const Graph &g: mst.graphs)
            os << g;
        return os;
    }

    vector<RecT> get_api_output() const {
        vector<RecT> ret;
        for (const auto &rec: records)
            for (const auto &p: rec.pc_threads)
                ret.emplace_back(rec.range, p.first, p.second);
        return ret;
    }

private:
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
        graphs.erase(remove_if(graphs.begin(), graphs.end(), [threshold](const Graph &g) {
            return g.get_n_false_sharing() < threshold;
        }), graphs.end());
    }

    void find_overlap(int m_id, size_t m_start, const std::unordered_map<Segment, vector<Record>> &bucket) {
        const Segment *prev = nullptr;
        for (const auto &it : bucket) {
            if (prev && prev->overlap(it.first)) {
                cerr << "Warning: offset range ";
                prev->shift_by(m_start, false).dump(cerr);
                cerr << " overlaps with ";
                it.first.shift_by(m_start, false).dump(cerr);
                cerr << " in malloc " << m_id << '\n';
            }
            prev = &it.first;
        }
    }

    vector<AddrRecord> records;
    vector<Graph> graphs;
    MallocInfo minfo;
    size_t malloc_fs;
    int m_id;
};

DetectPass::DetectPass(const string &in, const vector<string> &rest) :
        log_file(in),
        summary_file("summary/"+insert_suffix(in, "_summary"))/*,
        fsrStat(insert_suffix(in, "_fs_malloc"))*/ {
    assert(rest.size() <= 2);
    threshold = (!rest.empty()) ? stoul(rest[0]) : 100;
    string malloc_path = (rest.size() == 2) ? rest[1] : "mallocRuntimeIDs.txt";
    malloc_file.open(malloc_path);
    check_in_files();
}

void DetectPass::compute() {
    map<int, std::unordered_map<Segment, vector<Record>>> bins;
    map<int, MallocInfo> mallocs;
    Record next_r;
    MallocInfo next_m;
    while (malloc_file >> next_m)
        mallocs[next_m.id] = next_m;
    size_t i = 0;
    while (log_file >> next_r) {
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
        auto *mst = new MallocStorageT(p.first, mallocs[p.first], p.second, threshold);
        if (mst->valid())
            this->data.emplace(p.first, mst);
        else
            delete mst;
    }
    cout << "# of mallocs processed: " << bins.size() << '/' << bins.size() << endl;
    for (auto &pair: this->data) {
        //fsrStat.emplace(pair.second->get_n_false_sharing(), pair.first);
        summary_file << *(pair.second);
    }
    //fsrStat.print();
}

DetectPass::ApiT DetectPass::get_api_output() const {
    ApiT ret;
    for (const auto &p: this->data) {
        ret.emplace_back(p.second->get_api_output(), p.second->get_malloc_info());
    }
    return ret;
}

void DetectPass::print_result(const string &out) {
    ofstream outfile(out);
    const auto &apit = get_api_output();
    outfile << apit.size() << '\n';
    for (const auto &p: apit)
        outfile << p;
}

void DetectPass::check_in_files() {
    if (log_file.fail() || malloc_file.fail())
        throw std::invalid_argument("Can't open file\n");
}

DetectPass::~DetectPass() {
    for (const auto &p: this->data)
        delete p.second;
}

const char *DetectPass::optionals = "[threshold] [mallocfile]";
const size_t DetectPass::n_opt = 2;

MallocOutput::MallocOutput(std::vector<RecT> &&accesses, pair<PC, size_t> &&malloc) :
        accesses(move(accesses)), pc(malloc.first), size(malloc.second) {}

ostream &operator<<(ostream &os, const MallocOutput &mo) {
    os << mo.pc << ' ' << mo.size << ' ' << mo.accesses.size() << '\n';
    for (const auto &p: mo.accesses)
        os << get<0>(p) << ' ' << get<1>(p) << ' ' << get<2>(p) << '\n';
    return os;
}

istream &operator>>(istream &is, MallocOutput &mo) {
    size_t lines;
    is >> mo.pc >> mo.size >> lines;
    for (size_t i = 0; i < lines; i++) {
        Segment seg;
        PC pc;
        size_t thread;
        is >> seg >> pc >> thread;
        mo.accesses.emplace_back(seg, pc, thread);
    }
    return is;
}
