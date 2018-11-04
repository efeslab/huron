from sys import argv, stderr
from subprocess import check_output
from os import getcwd
from os.path import isabs, realpath, join
from collections import defaultdict


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

def get_lines_of_pcs(path, pcs, fix_cwd):
    def get_line_of_pc(path, pc):
        output = check_output(['addr2line', '-i', '-e', path, pc])
        src_file, row_str = output.decode('utf-8').strip().split(':')
        if src_file == '??' or row_str == '?':
            return None
        else:
            return (get_abs_path(src_file, fix_cwd), int(row_str))

    return [x for x in [get_line_of_pc(path, pc) for pc in pcs] if x is not None]


def src_list_to_pc(all_pcs, all_src_lines, src_list):
    all_src_to_pcs = defaultdict(list)
    for line, pc in zip(all_src_lines, all_pcs):
        all_src_to_pcs[line].append(pc)
    list_pcs = []
    # Col is not needed here; 
    # drop col, and we need to remove duplicate again
    file_row_list = list(sorted(set((file, row) for file, row, _ in src_list)))
    for src in file_row_list:
        if src not in all_src_to_pcs:
            raise "location %s is not found in source code" % src
        list_pcs.append((src, all_src_to_pcs[src]))
    return list_pcs


def main():
    if len(argv) != 3:
        print('Usage: %s <binary> <source_list>' % argv[0], file=stderr)
    _, bin_path, src_list_path = argv
    src_list = read_src_list(src_list_path, getcwd())
    all_pcs = get_bin_all_pcs(bin_path)
    all_lines = get_lines_of_pcs(bin_path, all_pcs, getcwd())
    src_pcs = src_list_to_pc(all_pcs, all_lines, src_list)
    pcs = [pc for _, pcs in src_pcs for pc in pcs]
    for pc in pcs:
        print(pc)


if __name__ == '__main__':
    main()
