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
            size_t id = to_unsigned<size_t>(fields[0]),
                    start = to_address(fields[1]),
                    size = to_unsigned<size_t>(fields[2]),
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
    size_t addr;
    int m_id, m_offset;
    uint16_t thread, size;
    PC pc;
    RW rw;

    Record() : addr(0), m_id(0), m_offset(0), thread(0), size(0), pc(0, 0), rw(0, 0) {}

    Segment get_addr_range() const {
        return {addr, addr + size};
    }

    bool cross_cacheline() const {
        return addr >> CACHELINE_BIT != (addr + size - 1) >> CACHELINE_BIT;
    }

    size_t cacheline() const {
        return addr >> CACHELINE_BIT;
    }

    static vector<Record> split_at_cacheline(Record rec) {
        vector<Record> records;
        while (rec.cross_cacheline()) {
            Record rec1 = rec;
            size_t l = rec.addr, cl_bound = ((l >> CACHELINE_BIT) + 1) << CACHELINE_BIT;
            rec1.size = (uint16_t) (cl_bound - l);
            rec.addr = cl_bound;
            rec.size -= rec1.size;
            records.push_back(rec1);
        }
        records.push_back(rec);
        return records;
    }

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

struct AddrRecord {
    unordered_map<uint32_t, RW> thread_rw;
    unordered_map<PC, RW> pc_rw;
    size_t clid, start, end;
    int malloc_id;

    AddrRecord(size_t _clid, size_t _start, size_t _end,
               vector<Record>::iterator records_begin, vector<Record>::iterator records_end) :
            thread_rw(8), pc_rw(20), clid(_clid) {
        malloc_id = records_begin->m_id;
        if (malloc_id != -1) {
            start = (size_t) records_begin->m_offset;
            end = start + _end - _start;
        } else {
            start = _start;
            end = _end;
        }
        for (auto it = records_begin; it != records_end; it++) {
            thread_rw[it->thread] += it->rw;
            pc_rw[it->pc] += it->rw;
        }
    }

    friend ostream &operator<<(ostream &os, const AddrRecord &rec) {
        os << '(' << hex << "0x" << rec.start << ", 0x" << rec.end << ')' << dec << ": ";
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

    static vector<AddrRecord> from_cacheline_records(size_t clid, const vector<Record> &records) {
        // We expect thousand/million records on each cache line, so an O(n) algorithm here:
        // Each record can be seen as a line segment[addr, addr + size)
        // First, get all _unique_ start and end points; they are breakpoints.
        bool breakpoints[CACHELINE + 1];
        memset(breakpoints, 0, CACHELINE + 1);
        for (const Record &rec: records) {
            auto seg = rec.get_addr_range();
            size_t l = seg.start - (clid << CACHELINE_BIT), r = seg.end - (clid << CACHELINE_BIT);
            assert(l >= 0 && l <= CACHELINE && r >= 0 && r <= CACHELINE && l < r);
            breakpoints[l] = breakpoints[r] = true;
        }
        vector<size_t> breakpoints_v;
        for (size_t i = 0; i < CACHELINE; i++)
            if (breakpoints[i])
                breakpoints_v.push_back(i + (clid << CACHELINE_BIT));

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
            addr_records.emplace_back(clid, l, r, this_range_records.begin(), this_range_records.end());
            // The number of breakpoints will not exceed CACHELINE_SIZE, so this is actually O(n).
        }
        addr_records.shrink_to_fit();
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

map<int, map<size_t, vector<Record>>> get_groups_from_log(ifstream &file) {
    map<int, map<size_t, vector<Record>>> map;
    Record next;
    while (file >> next) {
        if (next.cross_cacheline())
            for (auto splitted_rec: Record::split_at_cacheline(next))
                map[next.m_id][splitted_rec.cacheline()].push_back(move(splitted_rec));
        else
            map[next.m_id][next.cacheline()].push_back(next);
    }
    return map;
}

bool cl_single_threaded(const pair<const size_t, vector<Record>> &id_group) {
    unordered_set<size_t> threads;
    for (const auto &rec: id_group.second)
        threads.insert(rec.thread);
    return threads.size() == 1;
}

//void print_malloc(const string &path, const vector<Graph> &graphs) {
//    ofstream output(insert_suffix(path, "_malloc"));
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
//}

vector<Graph> calc_graphs(ostream &stats_stream,
                          int mid,
                          map<size_t, vector<Record>> &cl_groups,
                          size_t threshold = 100) {
    size_t n = cl_groups.size();

    size_t single_n = erase_count_if(cl_groups, cl_single_threaded);

    unordered_map<size_t, vector<AddrRecord>> addrrec_groups;
    for (const auto &pair: cl_groups)
        addrrec_groups[pair.first] = AddrRecord::from_cacheline_records(pair.first, pair.second);
    map<size_t, vector<Record>>().swap(cl_groups);

    vector<Graph> graphs;
    graphs.reserve(addrrec_groups.size());
    for (auto &pair: addrrec_groups)
        graphs.emplace_back(move(pair));
    unordered_map<size_t, vector<AddrRecord>>().swap(addrrec_groups);

    size_t small_n = remove_erase_count_if(graphs, [threshold](const Graph &g) { return g.estm_fs < threshold; });
    stats_stream << mid << ',' << n << ',' << single_n << ',' << small_n << ',' << graphs.size();
    return graphs;
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

    ofstream graphs_stream(insert_suffix(path, "_output"));
    ofstream stats_stream(insert_suffix(path, "_stats"));
    ofstream stats2_stream(insert_suffix(path, "_stats2"));
    map<size_t, size_t> fs_cl_ordering;

    auto groups = get_groups_from_log(file);
    size_t i = 0;
    for (auto &grp: groups) {
        i++;
        if (!(i % 1000))
            cout << i << '/' << groups.size() << endl;
        auto graphs = calc_graphs(stats_stream, grp.first, grp.second);
        if (!graphs.empty()) {
            sort(graphs.begin(), graphs.end());
            size_t malloc_fs = accumulate(graphs.begin(), graphs.end(), 0ul,
                                          [](size_t rhs, const Graph &lhs) { return rhs + lhs.estm_fs; });
            graphs_stream << "=================" << grp.first << "(" << malloc_fs << ")================\n";
            fs_cl_ordering.emplace(malloc_fs, grp.first);
            for (const Graph &g: graphs)
                graphs_stream << g;
        }
    }

    for (auto it = fs_cl_ordering.rbegin(); it != fs_cl_ordering.rend(); it++)
        stats2_stream << '#' << it->first << '@' << it->second << '\n';

    cout << "Format in file " << insert_suffix(path, "_stats") << ":\n"
         << "malloc_id, # of cachelines in total, # of single threaded cachelines, "
                 "# of graphs with too few estimated FS, "
                 "# of graphs left.\n";
}