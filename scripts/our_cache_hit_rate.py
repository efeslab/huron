from sys import argv, stderr
from subprocess import check_output
from os import getcwd
from os.path import isabs, realpath, join
from collections import defaultdict
from pprint import PrettyPrinter


def get_abs_path(path, cwd):
    if isabs(path):
        return realpath(path)
    else:
        return realpath(join(cwd, path))


def read_src_list(path, fix_cwd):
    def parse_line(line):
        if not line:
            return
        file, row_str, col_str = line.split(' ')
        return get_abs_path(file, fix_cwd), int(row_str), int(col_str)

    with open(path, 'r') as f:
        lines = f.readlines()
    return [parse_line(line) for line in lines]


def get_bin_all_pcs(path):
    output = check_output(['objdump', '-d', path])
    output_lines = output.decode('utf-8').split('\n')
    return [
        line.split(':')[0].lstrip()
        for line in output_lines
        if len(line) > 2 and line[0:2] == '  '
    ]

def get_pc_to_src_line(path, pcs, fix_cwd):
    def get_src_line(path, pc):
        output = check_output(['addr2line', '-i', '-e', path, pc])
        src_file, row_str = output.decode('utf-8').strip().split(':')
        if src_file == '??' or row_str == '?':
            return None
        else:
            return (get_abs_path(src_file, fix_cwd), int(row_str))

    return {int(pc, base=16): get_src_line(path, pc) for pc in pcs}


def src_list_to_pc(pc_to_lines, src_list):
    line_to_pcs = defaultdict(list)
    for pc, line in pc_to_lines.items():
        if line is not None:
            line_to_pcs[line].append(pc)
    list_pcs = []
    # Col is not needed here; 
    # drop col, and we need to remove duplicate again
    file_row_list = list(sorted(set((file, row) for file, row, _ in src_list)))
    for src in file_row_list:
        if src not in line_to_pcs:
            raise "location %s is not found in source code" % src
        list_pcs.append((src, line_to_pcs[src]))
    return list_pcs


def get_all_pcs(path):
    with open(path, 'r') as f:
        return [int(line.strip()) for line in f.readlines() if line]


def calc_pc_hit_rate(cache_pcs, hitm_pcs):
    cache_dict = dict()
    miss_dict = defaultdict(int)
    n_events = len(hitm_pcs)
    for x in cache_pcs:
        cache_dict[x] = 0
    for x in hitm_pcs:
        if x in cache_dict:
            cache_dict[x] += 1
        else:
            miss_dict[x] += 1
    cache_dict = {k: v / n_events for k, v in cache_dict.items()}
    miss_dict = {k: v / n_events for k, v in miss_dict.items()}
    return n_events, cache_dict, miss_dict


def main():
    if len(argv) != 4:
        print('Usage: %s <binary> <source_list> <hitm_event_pcs>' % argv[0], file=stderr)
    _, bin_path, src_list_path, hitm_path = argv
    src_list = read_src_list(src_list_path, getcwd())
    all_pcs = get_bin_all_pcs(bin_path)
    pc_to_lines = get_pc_to_src_line(bin_path, all_pcs, getcwd())
    src_pcs = src_list_to_pc(pc_to_lines, src_list)
    cache_pcs = [pc for _, pcs in src_pcs for pc in pcs]
    hitm_pcs = get_all_pcs(hitm_path)
    n_events, cache_dict, miss_dict = calc_pc_hit_rate(cache_pcs, hitm_pcs)
    hit_rate = sum(cache_dict.values())
    print('%d: %f' % (n_events, hit_rate))
    all_addr = sorted(
        [(hex(k) + '+', v) for k, v in cache_dict.items()] + 
        [(hex(k) + '*', v) for k, v in miss_dict.items()], 
        key=lambda p: p[1],
        reverse=True
    )
    pp = PrettyPrinter(indent=4)
    pp.pprint(all_addr)


if __name__ == '__main__':
    main()
