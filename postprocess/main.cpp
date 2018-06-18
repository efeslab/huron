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

    bool operator==(const Segment &rhs) const {
        return start == rhs.start && end == rhs.end;
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

struct AddrRecord {
    unordered_map<uint32_t, RW> thread_rw;
    unordered_map<PC, RW> pc_rw;
    size_t start, end;
    size_t malloc_start;
    int malloc_id;

    AddrRecord(size_t _start, size_t _end, int m_id, int m_offset, const vector<Record> &records) :
            malloc_id(m_id) {
        malloc_start = malloc_id == -1 ? 0 : _start - m_offset;
        start = _start - malloc_start;
        end = _end - malloc_start;
        for (const auto &rec: records) {
            thread_rw[rec.thread] += rec.rw;
            pc_rw[rec.pc] += rec.rw;
        }
    }

    pair<size_t, size_t> cachelines() const {
        size_t addr_start = malloc_start + start, addr_end_incl = malloc_start + end - 1;
        size_t start_cl = addr_start >> CACHELINE_BIT, end_cl = addr_end_incl >> CACHELINE_BIT;
        return make_pair(start_cl, end_cl);
    }

    friend ostream &operator<<(ostream &os, const AddrRecord &rec) {
        auto p = rec.cachelines();
        size_t addr_start = rec.malloc_start + rec.start, addr_end = rec.malloc_start + rec.end;
        size_t l = addr_start - (p.first << CACHELINE_BIT), r = addr_end - (p.second << CACHELINE_BIT);
        os << hex;
        if (p.first != p.second)
            os << "(" << p.first << "+0x" << l << ", " << p.second << "+0x" << r << ");";
        else
            os << "(" << "0x" << l << ", " << "0x" << r << ");";
        os << "m(" << "0x" << rec.start << ", 0x" << rec.end << ')' << ": ";
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

map<int, map<size_t, vector<AddrRecord>>> get_groups_from_log(ifstream &file) {
    map<int, unordered_map<Segment, vector<Record>>> bins;
    map<int, map<size_t, vector<AddrRecord>>> ret;
    Record next;
    while (file >> next) {
        auto key = Segment(next.addr, next.addr + next.size);
        bins[next.m_id][key].push_back(next);
    }
    for (const auto &p: bins) {
        for (const auto &p2: p.second) {
            const Record &ref = p2.second.front();
            size_t start = p2.first.start, end = p2.first.end;
            int m_id = ref.m_id, m_offset = ref.m_offset;
            AddrRecord arec(start, end, m_id, m_offset, p2.second);
            auto cls = arec.cachelines();
            assert(cls.first <= cls.second);
            for (size_t i = cls.first; i <= cls.second; i++)
                ret[m_id][i].push_back(arec);
        }
    }
    return ret;
}

bool cl_single_threaded(const pair<const size_t, vector<AddrRecord>> &id_group) {
    unordered_set<size_t> threads;
    for (const auto &rec: id_group.second)
        for (const auto &p: rec.thread_rw)
            threads.insert(p.first);
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
                          map<size_t, vector<AddrRecord>> &cl_groups,
                          size_t threshold = 100) {
    size_t n = cl_groups.size();

    size_t single_n = erase_count_if(cl_groups, cl_single_threaded);

    vector<Graph> graphs;
    graphs.reserve(cl_groups.size());
    for (auto &pair: cl_groups)
        graphs.emplace_back(move(pair));
    map<size_t, vector<AddrRecord>>().swap(cl_groups);

    size_t small_n = remove_erase_count_if(graphs, [threshold](const Graph &g) { return g.estm_fs < threshold; });
    stats_stream << mid << ',' << n << ',' << single_n << ',' << small_n << ',' << graphs.size() << '\n';
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

    ofstream graphs_stream(insert_suffix(path, "_summary"));
    ofstream stats_stream(insert_suffix(path, "_stats_malloc"));
    ofstream stats2_stream(insert_suffix(path, "_fs_malloc"));
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
