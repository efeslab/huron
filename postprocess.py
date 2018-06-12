#!/usr/bin/python3

import csv
import os
import sys
from collections import defaultdict
from itertools import groupby

CACHELINE_SIZE = 64


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
        if rangeStarts[i] <= addr < rangeEnds[i]:
            return mallocIds[i], rangeStarts[i], rangeEnds[i]
    return -1, -1, -1


class Record:
    def __init__(self, line):
        self.thread = int(line[0])
        self.addr = int(line[1], 16)
        self.func, self.inst, self.size, self.r, self.w = line[2:7]
        self.cacheline = self.addr // CACHELINE_SIZE

    def __str__(self):
        def print_addr(addr):
            return '0x{:02x}'.format(addr)
        cacheline_str = str(self.cacheline)
        thread_str = str(self.thread)
        addr_str = print_addr(self.addr)
        rest = [str(x) for x in [self.func, self.inst, self.size]]
        return ','.join([cacheline_str, thread_str, addr_str] + rest)

    def get_addr_range(self):
        if type(self.size) == str:
            self.size = int(self.size)
        return self.addr, self.addr + self.size

    def get_rw(self):
        if type(self.r) == str:
            self.r = int(self.r)
            self.w = int(self.w)
        return self.r, self.w

    def get_loc(self):
        if type(self.func) == str:
            self.func = int(self.func)
            self.inst = int(self.inst)
        return self.func, self.inst


class AddrRecord:
    def __init__(self, clid, records, start, end):
        self.thread_rw, self.pc_rw = defaultdict(lambda: [0, 0]), defaultdict(lambda: [0, 0])
        self.start, self.end = start, end
        self.m_id, self.m_start, self.m_end = get_malloc_id(self.start)
        self.clid = clid
        for rec in records:
            r, w = rec.get_rw()
            loc = rec.get_loc()
            self.thread_rw[rec.thread][0] += r
            self.thread_rw[rec.thread][1] += w
            self.pc_rw[loc][0] += r
            self.pc_rw[loc][1] += w

    def __str__(self):
        start_offset = self.start
        end_offset = self.end
        if self.m_id != -1:
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

    def get_malloc_info(self):
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

    def get_thread_ids(self):
        if self.is_read_only():
            return set()
        return set(self.thread_rw.keys())

    @staticmethod
    def get_total_rw(thread_rws):
        all_rws = [rw for d in thread_rws for rw in d.values()]
        return sum(rw[0] for rw in all_rws), sum(rw[1] for rw in all_rws)

    def is_read_only(self):
        for r, w in self.thread_rw.values():
            if w != 0:
                return False
        return True

    def thread_rw_excluding(self, exclude_threads):
        include_threads = set(self.thread_rw.keys()) - set(exclude_threads)
        return dict((t, self.thread_rw[t]) for t in include_threads)


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


class Graph:
    class GraphGroup:
        def __init__(self, threads, records):
            self.threads = threads
            self.records = records

        @staticmethod
        def rhs_rw_suffer_from_lhs(lhs, rhs):
            lhs_rw = AddrRecord.get_total_rw(l.thread_rw for l in lhs.records)
            rhs_minus_lhs_rw = AddrRecord.get_total_rw(r.thread_rw_excluding(lhs.threads) for r in rhs.records)
            lr, lw = lhs_rw
            rr, rw = rhs_minus_lhs_rw
            return min(lw, rr + rw)

        def __str__(self):
            import io
            with io.StringIO() as output:
                print(self.threads, file=output)
                for g0 in self.records:
                    print(g0, file=output)
                return output.getvalue()

    def __init__(self, clid, group):
        self.clid = clid
        self.v = group
        self.groups = self.get_thread_groups()

    def get_thread_groups(self):
        groups = []
        sorted_v = sorted(self.v, key=lambda x: x.get_thread_ids())
        for k, g in groupby(sorted_v, key=lambda x: x.get_thread_ids()):
            groups.append(self.GraphGroup(k, list(g)))
        groups = sorted(groups, key=lambda kg: kg.threads)
        return groups

    def __str__(self):
        import io
        with io.StringIO() as output:
            print('>>>%d<<<' % self.estm_false_sharing(), file=output)
            for grp in self.groups:
                print(grp, file=output, end='')
            print('\n', file=output)
            return output.getvalue()

    def get_malloc_info(self):
        mallocs = defaultdict(list)
        for v0 in self.v:
            mallocId, offset = v0.get_malloc_info()
            if mallocId != -1:
                mallocs[mallocId].append(offset)
        return mallocs

    def is_complete_graph(self):
        return len(self.groups) == 1

    def estm_false_sharing(self):
        total_rw = 0
        for i in range(len(self.groups)):
            max_rw = 0
            for j in range(i + 1, len(self.groups)):
                ij_rw = self.GraphGroup.rhs_rw_suffer_from_lhs(self.groups[i], self.groups[j])
                ji_rw = self.GraphGroup.rhs_rw_suffer_from_lhs(self.groups[j], self.groups[i])
                max_rw = max(max_rw, ij_rw, ji_rw)
            total_rw += max_rw
        return total_rw


def disp_thread_per_cl(graphs):
    graph_n_threads = [len(set(th for v in g.v for th in v.thread_rw.keys())) for g in graphs]
    print("Average thread per cacheline: %f" % (sum(graph_n_threads) / len(graph_n_threads)))


def print_final(path, graphs):
    suffix = '_output'
    root, ext = os.path.splitext(path)
    output_file = root + suffix + ext
    with open(output_file, 'w') as f:
        for g in graphs:
            print(g, file=f)


def print_malloc(path, graphs):
    suffix = '_malloc'
    root, ext = os.path.splitext(path)
    output_file = root + suffix + ext
    all_mallocs = defaultdict(list)
    for g in graphs:
        g_mallocs = g.get_malloc_info()
        for mallocId in g_mallocs:
            all_mallocs[mallocId].extend(g_mallocs[mallocId])
    with open(output_file, "w") as file:
        for malloc in sorted(all_mallocs.keys()):
            print(malloc, file=file)
            print(len(all_mallocs[malloc]), file=file)
            for i in sorted(all_mallocs[malloc]):
                print(i, file=file, end=' ')
            print('', file=file)


def append_csv_file(path, csv_line):
    suffix_ext = '_stable.csv'
    root, _ = os.path.splitext(path)
    csv_file = root + suffix_ext
    with open(csv_file, 'a') as f:
        print(csv_line, file=f)


def cl_multi_threaded(group):
    threads = set([rec.thread for rec in group])
    return len(threads) != 1


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

    graphs = [Graph(clid, group) for clid, group in addrrec_groups]
    noedge_n, graphs = 0, graphs  # filter_count(graphs, lambda g: not g.is_complete_graph())
    minimal_n, graphs = 0, graphs  # filter_count(graphs, lambda g: g.estm_false_sharing() >= 20)

    total = 0
    mallocs_fs = defaultdict(int)
    for g in graphs:
        fs = g.estm_false_sharing()
        total += fs
        involved_mallocs = set(v0.m_id for v0 in g.v)
        for m in involved_mallocs:
            mallocs_fs[m] += fs

    print("Estimated false sharing in total: %d" % total)
    print("For all mallocs: %s" % mallocs_fs)

    # disp_thread_per_cl(graphs)
    print_final(path, graphs)
    print_malloc(path, graphs)

    stats = n, single_n, noedge_n, minimal_n, n - noedge_n - minimal_n - single_n, len(mallocIds)
    print("""
%d cachelines in total. 
%d cachelines are single threaded (removed).
%d cachelines (graphs) have no false sharing (removed).
%d graphs have estm_false_sharing < 20 (removed).
Remain: %d.
%d mallocs occurred.
""" % stats)

    # csv_line = ','.join(str(x) for x in stats)
    # append_csv_file(path, csv_line)


main()
