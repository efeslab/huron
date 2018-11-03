import re
import sys

hitm_name = "mem_load_l3_hit_retired_xsnp_hitm"
regex = r"\s+([\d,]+)\s+cache-misses.+\n\s+([\d,]+)\s+" + hitm_name + r".+\s+([\d\.]+)"

with open(sys.argv[1], "r") as file:
    lines = file.readlines()
    valid_lines = []
    i = 0
    while i < len(lines):
        if 'Segmentation fault' in lines[i]:
            i += 8
        else:
            valid_lines.append(lines[i])
        i += 1
    filtered_str = ''.join(valid_lines)
    matches = re.finditer(regex, filtered_str, re.MULTILINE)
    hitms = []
    times = []
    for match in matches:
        _, hitm_str, time_str = match.groups()
        hitm_str = hitm_str.replace(",", "")
        hitms.append(int(hitm_str))
        times.append(float(time_str))
    hitms = hitms[:200]
    times = times[:200]
    avg_time = sum(times) / len(hitms)
    avg_hitm = sum(hitms) / len(times)
    print("samples = %d, avg_time = %.10f, avg_hitm = %d" % (len(hitms), avg_time, avg_hitm))
