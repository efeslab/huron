#!/usr/bin/python3

import csv
from collections import defaultdict
from itertools import groupby

CACHELINE_SIZE = 64


def print_addr(addr):
    return '0x{:02x}'.format(addr)


class Record:
    @staticmethod
    def _calc_cacheline_id(addr):
        return addr // CACHELINE_SIZE

    def __init__(self, line):
        self.thread = int(line[0])
        self.addr = int(line[1], 16)
        [self.func, self.inst, self.size] = [int(xstr) for xstr in line[2:5]]
        self.is_write = bool(line[5])
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
        self.clid = clid
        for rec in records:
            if rec.is_write:
                self.thread_rw[rec.thread][1] += 1
                self.pc_rw[(rec.func, rec.inst)][1] += 1
            else:
                self.thread_rw[rec.thread][0] += 1
                self.pc_rw[(rec.func, rec.inst)][0] += 1

    def __str__(self):
        return '(%s, %s)(%d)@%d: %s %s' % (
            print_addr(self.start),
            print_addr(self.end),
            self.end - self.start,
            self.clid,
            dict(self.thread_rw), dict(self.pc_rw)
        )

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


def filter_st_groups(groups):
    def is_single_threaded(group):
        threads = set([rec.thread for rec in group])
        return len(threads) == 1

    groups_filtered = []
    single_counter = 0
    for clid, group in groups:
        if is_single_threaded(group):
            single_counter += 1
        else:
            groups_filtered.append((clid, group))
    return single_counter, groups_filtered


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
                print(v0, file=output)
            for e0 in self.e:
                print(e0, file=output, sep=' ')
            print('\n\n\n', file=output)
            return output.getvalue()

    def is_complete_graph(self):
        return len(self.e) == len(self.v) * (len(self.v) - 1) / 2

    @staticmethod
    def filter_complete_graph(graphs):
        in_list = []
        out_counter = 0
        for g in graphs:
            if g.is_complete_graph():
                out_counter += 1
            else:
                in_list.append(g)
        return out_counter, in_list


def print_first_pass(groups):
    with open('__record__100_2_pass1.log', 'w') as f:
        for _, rows in groups:
            for rec in rows:
                print(str(rec), file=f)


def print_second_pass(addrrec_groups):
    with open('__record__100_2_pass2.log', 'w') as f:
        for clid, group in addrrec_groups:
            for addrrec in group:
                print(addrrec, file=f)


def sanity_check(groups, addrrec_groups):
    assert len(groups) == len(addrrec_groups)
    for (_, group), (_, addr_group) in zip(groups, addrrec_groups):
        addrrec_rw, group_rw = 0, 0
        for rec in addr_group:
            for r, w in rec.thread_rw.values():
                addrrec_rw += r + w
        group_rw = len(group)
        assert addrrec_rw == group_rw


def append_csv_file(csv_line):
    with open('__record__100_2_stable.csv', 'a') as f:
        print(csv_line, file=f)


def main():
    with open('__record__100_2.log', 'r') as f:
        groups = get_groups_from_log(f)

    n = len(groups)
    single_n, groups = filter_st_groups(groups)

    addrrec_groups = [
        (clid, AddrRecord.from_cacheline_records(clid, group))
        for clid, group in groups
    ]

    sanity_check(groups, addrrec_groups)

    graphs = [Graph(clid, group) for clid, group in addrrec_groups]
    noedge_n, graphs = Graph.filter_complete_graph(graphs)

    # print_first_pass(groups)
    # print_second_pass(addrrec_groups)

    with open('__record__100_2_output.log', 'w') as f:
        for g in graphs:
            print(g, file=f)

    print("""
%d cachelines in total. 
%d cachelines are single threaded (removed).
%d cacheline graphs have no edges (removed).
Remain: %d
""" % (n, single_n, noedge_n, n - single_n - noedge_n))

    csv_line = ','.join(str(x) for x in (n, single_n, noedge_n, n - single_n - noedge_n))
    append_csv_file(csv_line)


main()
