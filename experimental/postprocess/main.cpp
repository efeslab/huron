#include <algorithm>
#include <iostream>
#include <fstream>
#include <unordered_map>
#include <unordered_set>
#include <set>
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

    bool segment_overlap(const Segment &rhs) {
        return (start < rhs.end && end > rhs.start) || (rhs.start < end && rhs.end > start);
    }
};

struct MallocInfo {
    size_t start, end;
    uint32_t id;

    explicit MallocInfo(uint32_t _id = ~0U, size_t _start = ~0UL, size_t _end = ~0UL) :
            start(_start), end(_end), id(_id) {}

    static MallocInfo failed_value() {
        return MallocInfo();
    }

    bool failed() const {
        return id == ~0U;
    }

    bool operator<(const MallocInfo &rhs) const {
        return start < rhs.start;
    }
};

struct AllMallocs {
    vector<MallocInfo> mallocs;

    explicit AllMallocs(const string &path) {
        read_from(path);
    }

    void read_from(const string &path) {
        static CSVParser csv(3);
        ifstream malloc(path);
        string line;
        while (getline(malloc, line)) {
            const auto &fields = csv.read_csv_line(line);
            size_t id = to_integral<size_t>(fields[0]),
                    start = to_address(fields[1]),
                    size = to_integral<size_t>(fields[2]),
                    end = start + size;
            mallocs.emplace_back(id, start, end);
        }
        sort(mallocs.begin(), mallocs.end());
    }

    MallocInfo get_malloc_info(size_t addr) const {
        auto it = upper_bound(mallocs.begin(), mallocs.end(), MallocInfo(0, addr, 0));
        if (it == mallocs.begin())
            return MallocInfo::failed_value();
        it--;
        if (it->start <= addr && it->end > addr)
            return *it;
        else
            return MallocInfo::failed_value();
    }

    size_t size() const {
        return mallocs.size();
    }
};

struct Record {
    size_t addr, cacheline;
    uint16_t thread, size;
    PC pc;
    RW rw;

    Record() : addr(0), cacheline(0), thread(0), size(0), pc(0, 0), rw(0, 0) {}

    Segment get_addr_range() const {
        return {addr, addr + size};
    }

    friend istream &operator>>(istream &is, Record &rec) {
        static CSVParser csv(7);
        static string line;
        getline(is, line);
        if (line.empty())
            return is;
        const auto &fields = csv.read_csv_line(line);
        rec.thread = to_integral<uint16_t>(fields[0]);
        rec.addr = to_address(fields[1]);
        rec.cacheline = rec.addr / CACHELINE;
        rec.pc.func = to_integral<uint16_t>(fields[2]);
        rec.pc.inst = to_integral<uint16_t>(fields[3]);
        rec.size = to_integral<uint16_t>(fields[4]);
        rec.rw.r = to_integral<uint32_t>(fields[5]);
        rec.rw.w = to_integral<uint32_t>(fields[6]);

        return is;
    }
};

struct AddrRecord {
    unordered_map<uint32_t, RW> thread_rw;
    unordered_map<PC, RW> pc_rw;
    size_t clid, start, end;
    MallocInfo malloc;

    AddrRecord(size_t _clid, size_t _start, size_t _end,
               vector<Record>::iterator records_begin, vector<Record>::iterator records_end,
               const AllMallocs &mallocs) :
            thread_rw(8), pc_rw(20), clid(_clid), start(_start), end(_end) {
        malloc = mallocs.get_malloc_info(_start);
        if (!malloc.failed()) {
            start -= malloc.start;
            end -= malloc.start;
        }
        for (auto it = records_begin; it != records_end; it++) {
            thread_rw[it->thread] += it->rw;
            pc_rw[it->pc] += it->rw;
        }
    }

    friend ostream &operator<<(ostream &os, const AddrRecord &rec) {
        string malloc = (rec.malloc.id == MallocInfo::failed_value().id ? "X" : to_string(rec.malloc.id));
        os << malloc << "+(" << hex << "0x" << rec.start << ", 0x" << rec.end << ')' << dec << ": ";
        print_map(os, rec.thread_rw);
//        os << "  ";
//        print_map(os, rec.pc_rw);
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

    bool is_read_only() const {
        for (const auto &p: thread_rw)
            if (p.second.w != 0)
                return false;
        return true;
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

    static vector<AddrRecord> from_cacheline_records(size_t clid, const vector<Record> &records,
                                                     const AllMallocs &mallocs) {
        // We expect thousand/million records on each cache line, so an O(n) algorithm here:
        // Each record can be seen as a line segment[addr, addr + size)
        // First, get all _unique_ start and end points; they are breakpoints.
        bool breakpoints[CACHELINE];
        memset(breakpoints, 0, CACHELINE);
        for (const Record &rec: records) {
            auto seg = rec.get_addr_range();
            breakpoints[seg.start - (clid << CACHELINE_LSH)] = true;
            breakpoints[seg.end - (clid << CACHELINE_LSH)] = true;
        }
        vector<size_t> breakpoints_v;
        for (size_t i = 0; i < CACHELINE; i++)
            if (breakpoints[i])
                breakpoints_v.push_back(i + (clid << CACHELINE_LSH));

        vector<AddrRecord> addr_records;
        addr_records.reserve(breakpoints_v.size());
        // Then, iterate through breakpoints pairwise.
        for (size_t i = 1; i < breakpoints_v.size(); i++) {
            // For each pair, traverse all Records, find ones that overlap with[l, r).
            // If a Record is exactly on[l, r), use it as is.
            // Otherwise (the record range must be larger than[l, r)),
            // also use it as is, but only because AddrRecord() will ignore the value
            // in the record. This is equivalent to splitting this record into pieces.
            vector<Record> this_range_records;
            size_t l = breakpoints_v[i - 1], r = breakpoints_v[i];
            for (const Record &rec: records) {
                if (rec.get_addr_range().segment_overlap(Segment(l, r)))
                    this_range_records.push_back(rec);
            }
            if (this_range_records.empty())
                continue;
            addr_records.emplace_back(clid, l, r, this_range_records.begin(), this_range_records.end(), mallocs);
            // The number of breakpoints will not exceed CACHELINE_SIZE, so this is actually O(n).
        }
        return addr_records;
    }
};

struct Graph {
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

    size_t clid, estm_fs;
    vector<GraphGroup> groups;

    explicit Graph(pair<size_t, vector<AddrRecord>> &&records) :
            clid(records.first) {
        init_thread_groups(move(records.second));
        estm_fs = estm_false_sharing();
    }

    void init_thread_groups(vector<AddrRecord> &&v) {
        unordered_map<vector<bool>, vector<AddrRecord>> grouping_map;
        for (auto &rec: v)
            grouping_map[rec.get_thread_ids()].push_back(move(rec));
        for (auto &p: grouping_map)
            groups.emplace_back(move(p));
        // sort(groups.begin(), groups.end(), []() )
    }

    size_t estm_false_sharing() const {
        size_t total_rw = 0;
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

    bool operator<(const Graph &rhs) const {
        return clid < rhs.clid;
    }

    friend ostream &operator<<(ostream &os, const Graph &g) {
        os << ">>>0x" << hex << g.clid << dec << '(' << g.estm_fs << ")<<<\n";
        for (const auto &grp: g.groups)
            os << grp << '\n';
        os << '\n';
        return os;
    }
};

unordered_map<size_t, vector<Record>> get_groups_from_log(ifstream &file) {
    unordered_map<size_t, vector<Record>> map;
    Record next;
    while (file >> next)
        map[next.cacheline].push_back(next);
    return map;
}

bool cl_single_threaded(const pair<const size_t, vector<Record>> &id_group) {
    unordered_set<size_t> threads;
    for (const auto &rec: id_group.second)
        threads.insert(rec.thread);
    return threads.size() == 1;
}

void print_final(const string &path, const vector<Graph> &graphs) {
    ofstream output(insert_suffix(path, "_output"));
    for (const Graph &g: graphs)
        output << g;
}

void print_malloc(const string &path, const vector<Graph> &graphs) {
    ofstream output(insert_suffix(path, "_malloc"));
//    for (const Graph &g: graphs) {
//        g_mallocs = g.get_malloc_info()
//        for mallocId in g_mallocs:
//        all_mallocs[mallocId].extend(g_mallocs[mallocId])
//    }
//
//    for malloc in sorted(all_mallocs.keys()):
//    print(malloc, file=file)
//    print(len(all_mallocs[malloc]), file=file)
//    for i in sorted(all_mallocs[malloc]):
//    print(i, file=file, end=' ')
//    print('', file=file)
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        cerr << "Usage: " << argv[0] << " LOGFILE" << endl;
        return 1;
    }
    string path = argv[1];
    ifstream file(path);
    if (file.fail())
        return 1;
    std::ios_base::sync_with_stdio(false);

    auto malloc_info = AllMallocs("mallocIds.csv");

    auto groups = get_groups_from_log(file);
    size_t n = groups.size();

    size_t single_n = erase_count_if(groups, cl_single_threaded);

    unordered_map<size_t, vector<AddrRecord>> addrrec_groups;
    for (const auto &pair: groups)
        addrrec_groups[pair.first] = AddrRecord::from_cacheline_records(pair.first, pair.second, malloc_info);
    unordered_map<size_t, vector<Record>>().swap(groups);

    vector<Graph> graphs;
    graphs.reserve(addrrec_groups.size());
    for (auto &pair: addrrec_groups)
        graphs.emplace_back(move(pair));
    unordered_map<size_t, vector<AddrRecord>>().swap(addrrec_groups);

    size_t small_n = remove_erase_count_if(graphs, [](const Graph &g) { return g.estm_fs < 20; });

    sort(graphs.begin(), graphs.end());
    print_final(path, graphs);
    // print_malloc(path, graphs);

    printf("%lu cachelines in total.\n"
                   "%lu cachelines are single threaded (removed).\n"
                   "%lu graphs have estimated false_sharing < 20 (removed).\n"
                   "Remain: %lu.\n"
                   "%lu mallocs occurred.\n",
           n, single_n, small_n, graphs.size(), malloc_info.size()
    );
}