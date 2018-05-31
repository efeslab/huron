#!/usr/bin/python3

import csv
import os
import sys
from collections import defaultdict
from itertools import groupby

CACHELINE_SIZE = 64


def print_addr(addr):
    return '0x{:02x}'.format(addr)


def get_malloc_ids():
    f = open("mallocIds.csv", 'r')
    reader = csv.reader(f)
    mallocIds = []
    rangeStarts = []
    rangeEnds = []
    for row in reader:
        mallocId = int(row[0])
        start = int(row[1], 16)
        end = start + int(row[2])
        mallocIds.append(mallocId)
        rangeStarts.append(start)
        rangeEnds.append(end)
        pass
    f.close()
    return mallocIds, rangeStarts, rangeEnds


mallocIds, rangeStarts, rangeEnds = get_malloc_ids()


def get_malloc_id(addr):
    for i in range(len(mallocIds)):
        if rangeStarts[i] <= addr and addr < rangeEnds[i]:
            return mallocIds[i], rangeStarts[i], rangeEnds[i]
    return -1, -1, -1


class Record:
    @staticmethod
    def _calc_cacheline_id(addr):
        return addr // CACHELINE_SIZE

    def __init__(self, line):
        self.thread = int(line[0])
        self.addr = int(line[1], 16)
        [self.func, self.inst, self.size, self.r, self.w] = [int(xstr) for xstr in line[2:7]]
        self.cacheline = Record._calc_cacheline_id(self.addr)

    def __str__(self):
        cacheline_str = str(self.cacheline)
        thread_str = str(self.thread)
        addr_str = print_addr(self.addr)
        rest = [str(x) for x in [self.func, self.inst, self.size]]
        return ','.join([cacheline_str, thread_str, addr_str] + rest)

    def get_addr_range(self):
        return self.addr, self.addr + self.size


class AddrRecord:
    def __init__(self, clid, records, start, end):
        self.thread_rw, self.pc_rw = defaultdict(lambda: [0, 0]), defaultdict(lambda: [0, 0])
        self.start, self.end = start, end
        self.m_id, self.m_start, self.m_end = get_malloc_id(self.start)
        self.clid = clid
        for rec in records:
            self.thread_rw[rec.thread][0] += rec.r
            self.thread_rw[rec.thread][1] += rec.w
            self.pc_rw[(rec.func, rec.inst)][0] += rec.r
            self.pc_rw[(rec.func, rec.inst)][1] += rec.w

    def __str__(self):
        start_offset = self.start
        end_offset = self.end
        if self.m_id == -1:
            # do nothing
            pass
        else:
            start_offset -= self.m_start
            end_offset -= self.m_start
        return '(%d, %d)(%d)(%d)@%d: %s %s' % (
            start_offset,
            end_offset,
            self.end - self.start,
            self.m_id,
            # self.start // CACHELINE_SIZE == self.end // CACHELINE_SIZE,
            self.clid,
            dict(self.thread_rw), dict(self.pc_rw)
        )

    def getKeys(self):
        return set(self.pc_rw.keys())

    def getMallocInformation(self):
        if self.m_id == -1:
            return -1, -1
        return self.m_id, self.start - self.m_start

    @staticmethod
    def from_cacheline_records(clid, records):
        from itertools import tee

        def pairwise(iterable):
            """s -> (s0,s1), (s1,s2), (s2, s3), ..."""
            a, b = tee(iterable)
            next(b, None)
            return zip(a, b)

        def segment_overlap(seg1, seg2):
            return (
                    (seg1[0] < seg2[1] and seg1[1] > seg2[0]) or
                    (seg2[0] < seg1[1] and seg2[1] > seg1[0])
            )

        # We expect thousand/million records on each cache line, so an O(n) algorithm here:

        # Each record can be seen as a line segment [addr, addr + size)
        # First, get all _unique_ start and end points; they are breakpoints.
        breakpoints = sorted(list(set(lr for rec in records for lr in rec.get_addr_range())))

        addr_records = []

        # Then, iterate through breakpoints pairwise (see above function).
        for l, r in pairwise(breakpoints):
            # For each pair, traverse all Records, find ones that overlap with [l, r).
            # If a Record is exactly on [l, r), use it as is.
            # Otherwise (the record range must be larger than [l, r)),
            # also use it as is, but only because AddrRecord() will ignore the value
            # in the record. This is equivalent to splitting this record into pieces.
            this_range_records = [rec for rec in records
                                  if segment_overlap((l, r), rec.get_addr_range())]
            if not this_range_records:
                continue
            addr_records.append(AddrRecord(clid, this_range_records, l, r))
        # The number of breakpoints will not exceed CACHELINE_SIZE, so this is actually O(n).
        return addr_records

    def get_total_rw(self):
        return sum(x for rw in self.thread_rw.values() for x in rw)

    def is_read_only(self):
        for r, w in self.thread_rw.values():
            if w != 0:
                return False
        return True

    def is_true_shared(self):
        return len(self.thread_rw.keys()) > 1

    @staticmethod
    def read_only_2(rec1, rec2):
        return rec1.is_read_only() and rec2.is_read_only()

    @staticmethod
    def same_thread_affinity(rec1, rec2):
        return rec1.thread_rw.keys() == rec2.thread_rw.keys()

    @staticmethod
    def thread_equivalence(rec1, rec2):
        return AddrRecord.read_only_2(rec1, rec2) or \
               AddrRecord.same_thread_affinity(rec1, rec2)


def get_groups_from_log(file):
    reader = csv.reader(file)
    records = sorted([Record(row) for row in reader], key=lambda r: r.cacheline)
    groups = [
        (key, [item for item in data]) for (key, data) in groupby(records, lambda r: r.cacheline)
    ]
    return groups


def filter_count(objs, in_pred):
    in_list = []
    out_counter = 0
    for o in objs:
        if in_pred(o):
            in_list.append(o)
        else:
            out_counter += 1
    return out_counter, in_list


def cl_multi_threaded(group):
    threads = set([rec.thread for rec in group])
    return len(threads) != 1


class Edge:
    def __init__(self, f, t, w):
        self.f = f
        self.t = t
        self.w = w

    def __str__(self):
        return '%d<-->%d(%d)' % (self.f, self.t, self.w)


class Graph:
    def __init__(self, clid, group):
        self.clid = clid
        self.v = group
        edges = []
        for i in range(len(group)):
            for j in range(i + 1, len(group)):
                if AddrRecord.thread_equivalence(group[i], group[j]):
                    edges.append(Edge(i, j, 0))
        self.e = edges

    def __str__(self):
        import io
        with io.StringIO() as output:
            for v0 in self.v:
                print(v0, file=output, end='\n')
            for e0 in self.e:
               print(e0, file=output)
            print('\n', file=output)
            return output.getvalue()

    def getPCInformation(self):
        dict = {}
        for v0 in self.v:
            keys = v0.getKeys()
            for key in keys:
                if key not in dict:
                    dict[key] = True
        return set(dict.keys())

    def getMallocInformation(self):
        mallocs = {}
        for v0 in self.v:
            mallocId, offset = v0.getMallocInformation()
            if mallocId == -1:
                continue
            if mallocId not in mallocs:
                mallocs[mallocId] = {offset: True}
            else:
                mallocs[mallocId][offset] = True
        return mallocs

    def is_complete_graph(self):
        return len(self.e) == len(self.v) * (len(self.v) - 1) / 2


def is_4equalnodes(g):
    return len(g.v) == 4 and all(v.end - v.start == 16 for v in g.v)


def is_all_nodes_rw_2(g):
    return all(v.get_total_rw() == 2 for v in g.v)


def print_first_pass(path, groups):
    suffix = '_pass1'
    root, ext = os.path.splitext(path)
    first_pass_file = root + suffix + ext
    with open(first_pass_file, 'w') as f:
        for _, rows in groups:
            for rec in rows:
                print(str(rec), file=f)


def print_second_pass(path, addrrec_groups):
    suffix = '_pass2'
    root, ext = os.path.splitext(path)
    second_pass_file = root + suffix + ext
    with open(second_pass_file, 'w') as f:
        for clid, group in addrrec_groups:
            for addrrec in group:
                print(addrrec, file=f)


def print_final(path, graphs):
    suffix = '_output'
    root, ext = os.path.splitext(path)
    output_file = root + suffix + ext
    with open(output_file, 'w') as f:
        for g in graphs:
            print(g, file=f)
    # dict = {}
    # for g in graphs:
    #     keys = g.getPCInformation()
    #     for key in keys:
    #         if key not in dict:
    #             dict[key] = True
    # pcs = sorted(list(dict.keys()))
    # with open(output_file, 'w') as f:
    #     for pc in pcs:
    #         print(str(pc[0]) + ' ' + str(pc[1]), file=f)


def print_malloc_final(path, graphs):
    suffix = '_malloc'
    root, ext = os.path.splitext(path)
    output_file = root + suffix + ext
    mallocs = {}
    for g in graphs:
        malloc = g.getMallocInformation()
        for mallocId in malloc:
            if mallocId not in mallocs:
                mallocs[mallocId] = malloc[mallocId]
            else:
                for offset in malloc[mallocId]:
                    if offset not in mallocs[mallocId]:
                        mallocs[mallocId][offset] = True
    malloc_prims = dict(zip(mallocIds, zip(rangeStarts, rangeEnds)))
    with open(output_file, "w") as file:
        for malloc in sorted(mallocs.keys()):
            start, end = malloc_prims[malloc]
            size = end - start
            print("%d, %d (%d)" % (malloc, len(mallocs[malloc]), size), file=file)
            # for i in sorted(mallocs[malloc].keys()):
            #     print(i, file=file, end=' ')
            print('', file=file)


def sanity_check(groups, addrrec_groups):
    assert len(groups) == len(addrrec_groups)
    for (_, group), (_, addr_group) in zip(groups, addrrec_groups):
        addrrec_rw, group_rw = 0, 0
        for rec in addr_group:
            for r, w in rec.thread_rw.values():
                addrrec_rw += r + w
        group_rw = len(group)
        assert addrrec_rw == group_rw


def append_csv_file(path, csv_line):
    suffix_ext = '_stable.csv'
    root, _ = os.path.splitext(path)
    csv_file = root + suffix_ext
    with open(csv_file, 'a') as f:
        print(csv_line, file=f)


def main():
    args = sys.argv
    if len(args) != 2:
        print("Usage: %s LOGFILE" % args[0])
        sys.exit(1)
    path = args[1]

    with open(path, 'r') as f:
        groups = get_groups_from_log(f)

    n = len(groups)
    single_n, groups = filter_count(groups, lambda group: cl_multi_threaded(group[1]))

    addrrec_groups = [
        (clid, AddrRecord.from_cacheline_records(clid, group))
        for clid, group in groups
    ]

    # sanity_check(groups, addrrec_groups)

    graphs = [Graph(clid, group) for clid, group in addrrec_groups]
    # print(sum(v.get_total_rw() for g in graphs for v in g.v))
    noedge_n, graphs = filter_count(graphs, lambda g: not g.is_complete_graph())
    # print(sum(v.get_total_rw() for g in graphs for v in g.v))
    minimal_n, graphs = filter_count(
        graphs,
        lambda g: not is_4equalnodes(g) or not is_all_nodes_rw_2(g)
    )
    # print(sum(v.get_total_rw() for g in graphs for v in g.v))
    graph_n_threads = [len(set(th for v in g.v for th in v.thread_rw.keys())) for g in graphs]
    print("Average thread per cacheline: %f\n" % (sum(graph_n_threads) / len(graph_n_threads)))
    # print_first_pass(path, groups)
    # print_second_pass(path, addrrec_groups)
    print_final(path, graphs)
    print_malloc_final(path, graphs)

    stats = n, single_n, noedge_n, minimal_n, n - single_n - noedge_n - minimal_n, len(mallocIds)
    print("""
%d cachelines in total. 
%d cachelines are single threaded (removed).
%d cacheline graphs have no edges (removed).
%d graphs are 4 symmetric nodes with r, w = 1, 1
Remain: %d.
%d mallocs occurred.
""" % stats)

    csv_line = ','.join(str(x) for x in stats)
    append_csv_file(path, csv_line)


main()
