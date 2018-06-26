import csv
locations={}
mallocStart={}
with open('mallocRuntimeIDs.txt', 'r') as mallocFile:
    reader = csv.reader(mallocFile, delimiter=',')
    for row in reader:
        mallocStart[int(row[0])] = {}
        mallocStart[int(row[0])]['start'] =int(row[1])
        mallocStart[int(row[0])]['size'] =int(row[2])
        mallocStart[int(row[0])]['func'] =int(row[3])
        mallocStart[int(row[0])]['inst'] =int(row[4])
    pass
with open('location.txt', 'r') as locationFile:
    '''
    reader = csv.reader(locationFile, delimiter=' ')
    for row in reader:
        locations[(int(row[0]), int(row[1]))]=[]
        pass
    '''
    numMallocs = int(locationFile.readline())
    for i in range(numMallocs):
        mallocId = int(locationFile.readline())
        locations[mallocId] = {}
        numPCs = int(locationFile.readline())
        for j in range(numPCs):
            row = locationFile.readline().strip().split()
            locations[mallocId][(int(row[0]), int(row[1]))]=[]
    pass
# print locations
table={}
with open('layout.txt', 'r') as layoutFile:
    numMallocs = int(layoutFile.readline())
    print numMallocs
    for i in range(numMallocs):
        mallocId = int(layoutFile.readline())
        table[mallocId] = {}
        originalSize = int(layoutFile.readline())
        modifiedSize = int(layoutFile.readline())
        print mallocStart[mallocId]['func'], mallocStart[mallocId]['inst'], originalSize, modifiedSize
        numLayoutEntries = int(layoutFile.readline())
        for j in range(numLayoutEntries):
            line = layoutFile.readline()
            row = line.strip().split()
            table[mallocId][int(row[0])]=int(row[1])
            pass
        pass
    pass
with open('record.log', 'r') as traceFile:
    reader = csv.reader(traceFile, delimiter=',')
    for row in reader:
        mallocId = int(row[2])
        if mallocId in locations and (int(row[4]), int(row[5])) in locations[mallocId]:
            previousOffset = int(row[1], 16)-mallocStart[mallocId]['start']
            newOffset = table[mallocId][previousOffset]
            locations[mallocId][(int(row[4]), int(row[5]))].append(( int(row[0]), previousOffset, newOffset))
        pass
    pass
for mallocKey in sorted(locations.keys()):
    locationMallocs = locations[mallocKey]
    for location in sorted(locationMallocs.keys()):
        # print '-------'
        print location[0], location[1], len(locationMallocs[location])
        # print '-------'
        for row in sorted(locationMallocs[location]):
            print row[0], row[1], row[2]
        # print '-------'
