#include <unordered_map>  //for unordered_map
#include <iostream>
#include <time.h>
#include <stdio.h>
#include <queue>          // for priority_queue
#include <vector>         // for vector
#include <iomanip>
#include <assert.h>
#include <string.h>
#include <unistd.h> 
#include <sys/types.h> 
#include <sys/wait.h>
#include <fstream> 
#include "SharingEventType.h"
#include "DataAccess.h"
#include "HotSpotWindow.h"
#include "PCCounterComparer.h"
#include "Helpers.h"
#include "CategoryCounter.h"
#include <unordered_set>
#include "laser_api.h"

#include <chrono>

#define EventOutputThreshold 10000
#define NumberOfWindowsToTrack 5
#define UseAggregatedOutput true
#define LaserThreshold 50
#define WWThreshold 250

#define LaunchThreshold 1000

using namespace std;
using std::chrono::high_resolution_clock;

class FSD
{
public:
  void HandleEvent(DataAccess& da);

  bool ShouldLaunchProtection();
  bool DegradeToSheriff();

  void DumpDetectionResult(char* app, char* fsd);
  //you are responsible for freeing the space.
  bool RetrieveHotPages(std::vector<LaserEvent*> *hotPages);
  int SamplePeriod;
  int WindowSize;
  FSD(bool _enableWindowing = true);
  ~FSD();
  
private:
  unordered_map<uint64_t, int> addressCounter;
  unordered_map<uint64_t, DataAccess> lastAccessMap;
  unordered_map<uint64_t, int> pcCounter;
  unordered_map<uint64_t, CategoryCounter> typeCounter;
  unordered_map<uint64_t, uint64_t> addrToPC;

  unordered_map<uint64_t, int> hotPagesWindow;
  unordered_map<uint64_t, int> hotPCsWindow;

  unsigned long WWS;
  unsigned long RWS;
  unsigned long hitmCounter;
  bool enableWindowing;

  high_resolution_clock::time_point initTime;
  high_resolution_clock::time_point windowTime;
};

FSD::FSD(bool _enableWindowing) : enableWindowing(_enableWindowing), SamplePeriod(1), WindowSize(0), WWS(0), RWS(0) {
  initTime = high_resolution_clock::now();
  windowTime = high_resolution_clock::now();
}

void FSD::DumpDetectionResult(char *app, char* fsd)
{
  std::cout << "Doing detector output\n";

  if(pcCounter.size() == 0){
    return;
  }

  string Path(app);
  string savePath;// = split(Path,'/').back();
  string stringCmd;
  stringCmd.append("mkdir ").append(fsd).append("/results 2> /dev/null");
  execArbitraryShellCmd(stringCmd.c_str());
  savePath.append(fsd);
  savePath.append("/results/");
  savePath.append(split(Path, '/').back());
  savePath.append(".csv");
  cout << "|--->FSD: Save path is " << savePath << endl;
  FILE* accuracyFd = fopen(savePath.c_str(), "w+");
  FILE* translationFd = fopen("lines.txt", "w+");

  if(accuracyFd == nullptr || translationFd == nullptr){
    cout << "Unable to open output files\n";
    return;
  }

  //string pcs = "";
  priority_queue<pair<uint64_t, int>, vector<pair<uint64_t, int>>, PCCounterComparer> priorityQueue;

  cout << "Total PCS: " << pcCounter.size() << "\n";
  cout << "Total HITMs: " << hitmCounter << "\n";

  auto it = pcCounter.begin();
  vector<int> pcHitCounts;
  vector<CategoryCounter> typeCounts;
  unordered_map<string, uint64_t> lineAggregator;
  unordered_map<string, CategoryCounter> typeAggregator;
  while (it != pcCounter.end()) {
    if (priorityQueue.size() < NumberOfHotSpotPCsToTrack) {
      priorityQueue.push(*it);
    } else {
      priorityQueue.pop();
      priorityQueue.push(*it);
    }
    it++;
  }
  if (priorityQueue.size() == 0) {
    fprintf(accuracyFd, "===\n");
    fflush(accuracyFd);
    fclose(accuracyFd);
    string setPermission ("sudo chmod 666 ");
    setPermission.append(savePath);
    execArbitraryShellCmd(setPermission.c_str());
    return;
  }
  for (int i = 0; i < NumberOfHotSpotPCsToTrack; i++) {
    if (priorityQueue.size() == 0) {
      break;
    }
    pair<uint64_t, int> p = priorityQueue.top();
    fprintf(translationFd, uint64ToHexString(p.first).c_str());
    fprintf(translationFd, "\n");
    //cout<<"Considering "<<hex<<p.first<<" with count "<<dec<<p.second<<endl;
    pcHitCounts.push_back(p.second);
    if (typeCounter.find(p.first) == typeCounter.end()) {
      typeCounts.push_back(CategoryCounter());//pad a 0 counter.
    } else {
      typeCounts.push_back(typeCounter[p.first]);
    }
    priorityQueue.pop();
  }
  fflush(translationFd);
  fclose(translationFd);

  high_resolution_clock::time_point current = high_resolution_clock::now();
  auto windowDuration = current - initTime;
  unsigned long totalTime = windowDuration.count() / 1000000000;
  totalTime = (totalTime == 0) ? 1 : totalTime;

  printf("Total time: %lu\n",totalTime);

  int pid = getpid();
  char exe[200];
  sprintf(exe,"/proc/%d/exe",pid);
  string cmd = "cat lines.txt | /usr/bin/addr2line -e ";
  cmd.append(exe);
  std::cout << "Running cmd: " << cmd << "\n";
  istringstream input(execArbitraryShellCmd(cmd.c_str()));
  assert(pcHitCounts.size() == typeCounts.size());
  for (int i = 0; i < pcHitCounts.size(); i++) {
    string line;
    getline(input, line);
    if (lineAggregator.end() == lineAggregator.find(line)) {
      pair<string, uint64_t> defaultCount(line, 0);
      pair<string, CategoryCounter> defaultEvents(line, CategoryCounter());
      lineAggregator.insert(defaultCount);
      typeAggregator.insert(defaultEvents);
    }
    lineAggregator[line] += pcHitCounts[i];
    typeAggregator[line] = typeAggregator[line].Merge(typeCounts[i]);//aggregate.
  }
  for (auto it = lineAggregator.cbegin(); it != lineAggregator.cend(); ++it) {
    fprintf(accuracyFd, "%s,%llu,%s\n", it->first.c_str(), it->second / totalTime,EventTypeToPrintableName(typeAggregator[it->first].DominantType()).c_str());

    if(it->second / totalTime > 1000){
      printf("%s,%llu,%s\n", it->first.c_str(), it->second / totalTime, EventTypeToPrintableName(typeAggregator[it->first].DominantType()).c_str());
    }
  }
  fprintf(accuracyFd, "===\n");
  fflush(accuracyFd);
  fclose(accuracyFd);
  string setPermission ("sudo chmod 666 ");
  setPermission.append(savePath);
  execArbitraryShellCmd(setPermission.c_str());
}

bool FSD::ShouldLaunchProtection()
{
  high_resolution_clock::time_point current = high_resolution_clock::now();
  auto windowDuration = current - windowTime;
  unsigned long seconds = windowDuration.count() / 1000000000;
  seconds = (seconds != 0) ? seconds : 1;

  std::cout << "Count: " << hitmCounter / seconds << ", Threshold: " << LaunchThreshold << "\n";

  return hitmCounter / seconds > LaunchThreshold;
}

bool FSD::DegradeToSheriff()
{
  if(RWS == 0){
    return true;
  }

  return WWS > WWThreshold;
}

bool FSD::RetrieveHotPages(std::vector<LaserEvent*> *hotPages)
{
  high_resolution_clock::time_point current = high_resolution_clock::now();

  auto windowDuration = current - windowTime;

  for(auto pair : hotPagesWindow){
    uint64_t addr = pair.first;
    int count = pair.second;

    if(count >= LaserThreshold){
      LaserEvent *event = new LaserEvent;
      event->Addr = addr;
      event->PC = addrToPC[addr];
      std::cout << "Count: " << count << ", PC: " << event->PC << ", Addr: " << event->Addr << "\n";
      hotPages->push_back(event);
    }
  }
  
  return true;
}

void FSD::HandleEvent(DataAccess& da)
{
  if (enableWindowing && WindowSize != 0 && 
      hitmCounter % WindowSize == 0) {
    //clear detection.
    lastAccessMap.clear();
    hotPagesWindow.clear();
    hotPCsWindow.clear();
    windowTime = high_resolution_clock::now();
  }

  unordered_map<uint64_t, DataAccess>::const_iterator got = 
    lastAccessMap.find(da.LineAddress(da.Address));
  if (addressCounter.find(da.Address) == addressCounter.end()){
    addressCounter[da.Address] = 0;
  }
  addressCounter[da.Address] += SamplePeriod;

  SharingEventType eventType = SharingEventType::Other;
  if (got == lastAccessMap.end()) {
    pair<uint64_t, DataAccess> temp(da.LineAddress(da.Address), da);
    lastAccessMap.insert(temp);
  } else {
    DataAccess earlier = lastAccessMap[da.LineAddress(da.Address)];
    eventType = DataAccess::DetermineEventType(earlier, da);
    int index = EventTypeToIndex(eventType);
    if (index >= 0) {
      //start event categorization
      if (typeCounter.find(da.PC) == typeCounter.end()) {
	typeCounter[da.PC] = CategoryCounter();
      }
      typeCounter[da.PC].Increment(eventType,SamplePeriod);
    }
    lastAccessMap[da.LineAddress(da.Address)] = da;
  }

  //Increment PC counter whenever an event is there.
  if (pcCounter.find(da.PC) == pcCounter.end()) {
    pcCounter[da.PC] = 0;
  }
  pcCounter[da.PC] += SamplePeriod;

  uint64_t page = da.Address & (0xFFFFFFFFFFFFE000);

  if(eventType != SharingEventType::TSWW && eventType != SharingEventType::TSWR){
    if(hotPCsWindow.count(da.PC) == 0){
      hotPCsWindow[da.PC] = 0;
    }
    hotPCsWindow[da.PC] += SamplePeriod;
    
    if (hotPagesWindow.count(page) == 0) {
      hotPagesWindow[page] = 0;
    }
    hotPagesWindow[page] += SamplePeriod;
  }

  addrToPC[page] = da.PC;

  if(eventType == SharingEventType::FSWW || 
     eventType == SharingEventType::TSWW ||
     eventType == SharingEventType::Other){
    // If we have a lot of HITM events that indicate WW sharing, we need
    // to simply degrade to sheriff instead of launching protection as normal
    WWS += SamplePeriod;
  }else{
    RWS+= SamplePeriod;
  }

  hitmCounter++;
}
