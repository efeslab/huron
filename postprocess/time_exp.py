from StringIO import StringIO
import csv
import sys

if len(sys.argv) > 1:
  power = int(sys.argv[1])
else:
  exit(-1)


# full_file = {}
first_cycle = 0

with open('1.log', 'r') as csv_file:
  #for i in range(10):
  #  line=csv_file.readline()
  for line in csv_file:
    for row in csv.reader(StringIO(line), delimiter=','):
      current_cycle=int(row[9])
      if first_cycle == 0:
        first_cyle = current_cycle
      current_cycle = current_cycle - first_cycle
      current_cycle /= 10**power
      #if current_cycle not in full_file:
      tmp_file=open(''+`power`+'/'+`current_cycle`+'.txt', 'a')
      print >>tmp_file, line,
      tmp_file.close()
'''
for key in full_file.keys():
  # print key
  # f=open(''+`power`+'/'+`key`+'.txt', 'w')
  # print >>f, full_file[key],
  full_file[key].close()
'''
'''
with open('1.log', 'r') as csv_file:
  log_reader = csv.reader(csv_file)
  for row in log_reader:
    current_cycle = int(row[9])
    if first_cycle == 0:
      first_cycle = current_cycle
    current_cycle = current_cycle - first_cycle
    current_cycle /= 1000
    if current_cycle not in full_file:
      full_file[current_cycle]=1
    else:
      full_file[current_cycle]+=1
print len(full_file.keys())
cycles = sorted(full_file.keys())
print cycles[0],cycles[-1]
print full_file[cycles[0]],full_file[cycles[-1]]
'''
