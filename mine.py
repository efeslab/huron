import re
import sys

regex = r"\s+([\d,]+)\s+cache-misses.+\n\s+([\d,]+)\s+mem_load_uops_l3_hit_retired_xsnp_hitm.+\s+([\d\.]+)"

test_str = ""

with open(sys.argv[1], "r") as file:
  test_str = file.read()

matches = re.finditer(regex, test_str, re.MULTILINE)

for matchNum, match in enumerate(matches):
  matchNum = matchNum + 1
  # print ("Match {matchNum} was found at {start}-{end}: {match}".format(matchNum = matchNum, start = match.start(), end = match.end(), match = match.group()))
  for groupNum in range(0, len(match.groups())):
    groupNum = groupNum + 1
    print "{group};".format(group = match.group(groupNum)),
  print ''
