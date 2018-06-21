#include <map>
#include <vector>
#include <stdio.h>
#include <random>

using namespace std;

#define READ_WEIGHT 1
#define WRITE_WEIGHT 1

class RWCounts
{
public:
  unsigned int readCounts;
  unsigned int writeCounts;
  RWCounts(unsigned r=0, unsigned w=0)
  {
    readCounts = r;
    writeCounts = w;
  }
};
class FeatureVector
{
public:
  map<unsigned, RWCounts *> features;
  unsigned addressStart;
  unsigned rwSize;
  void print()
  {
    printf("%u %u\n", addressStart, rwSize);
    for(map<unsigned, RWCounts *>::iterator it = features.begin(); it != features.end(); it++)
    {
      RWCounts * rwc = it->second;
      printf("%u %u %u\n",it->first, rwc->readCounts, rwc->writeCounts);
    }
  }
};

unsigned getDistance(FeatureVector *centroid, FeatureVector *fv)
{
  unsigned distance = 0;
  for(map<unsigned, RWCounts*>::iterator it = centroid->features.begin(); it!=centroid->features.end(); it++)
  {
    unsigned threadId = it->first;
    if(fv->features.find(threadId) == fv->features.end())
    {
      distance += (it->second->readCounts * READ_WEIGHT);
      distance += (it->second->writeCounts * WRITE_WEIGHT);
    }
    else
    {
      RWCounts *rwc = fv->features[threadId];
      int readDistance = ((int) it->second->readCounts) - ((int) rwc->readCounts);
      if(readDistance < 0)readDistance = - readDistance;
      int writeDistance = ((int) it->second->writeCounts) - ((int) rwc->writeCounts);
      if(writeDistance < 0)writeDistance = - writeDistance;
      distance += (readDistance * READ_WEIGHT);
      distance += (writeDistance * WRITE_WEIGHT);
    }
  }
  return distance;
}

unsigned getClosestCentroid(vector<FeatureVector *> &centroids, FeatureVector *fv)
{
  unsigned result = 0;
  unsigned minDistance = 0;
  for(unsigned i = 0; i < centroids.size(); i++)
  {
    unsigned distance = getDistance(centroids[i], fv);
    if (i == 0)
    {
      minDistance = distance;
    }
    else 
    {
      if(distance < minDistance)
      {
        result = i;
        minDistance = distance;
      }
    }
  }
  return result;
}

void /*vector<unsigned>*/ kmeans(unsigned k, vector<FeatureVector *> dataset)
{
  //initialize
  map<unsigned, RWCounts*> presentThreadIds;
  map<unsigned, RWCounts*>::iterator findInPresentThreadIds;
  for(int i = 0; i < dataset.size(); i++)
  {
    FeatureVector * fv = dataset[i];
    for(map<unsigned, RWCounts *>::iterator it = fv->features.begin(); it!= fv->features.end(); it++)
    {
      unsigned threadId = it->first;
      findInPresentThreadIds = presentThreadIds.find(threadId);
      if(findInPresentThreadIds == presentThreadIds.end())
      {
        presentThreadIds[threadId]=new RWCounts(it->second->readCounts, it->second->writeCounts);
      }
      else {
        if((findInPresentThreadIds->second->readCounts) < (it->second->readCounts) ) findInPresentThreadIds->second->readCounts = it->second->readCounts;
        if((findInPresentThreadIds->second->writeCounts) < (it->second->writeCounts) ) findInPresentThreadIds->second->writeCounts = it->second->writeCounts;
      }
    }
  }
  vector<FeatureVector *> centroids;
  for(int i = 0; i < k; i++)
  {
    centroids.push_back(new FeatureVector());
  }
  std::default_random_engine generator;
  for(findInPresentThreadIds = presentThreadIds.begin(); findInPresentThreadIds!= presentThreadIds.end(); findInPresentThreadIds++)
  {
    unsigned threadId = findInPresentThreadIds->first;
    std::uniform_int_distribution<unsigned int> distributionRead(0,findInPresentThreadIds->second->readCounts);
    std::uniform_int_distribution<unsigned int> distributionWrite(0,findInPresentThreadIds->second->writeCounts);
    for(int i = 0; i < k; i++)
    {
      centroids[i]->features[threadId] = new RWCounts(distributionRead(generator), distributionWrite(generator));
    }
  }
  /*for(int i =0; i < k; i++)
  {
    centroids[i]->print();
  }*/
  /*vector<unsigned> resultMap;
  for()*/
}

vector<FeatureVector *> readData(FILE *fp)
{
  vector<FeatureVector *> dataset;
  int mallocSize, numItems;
  fscanf(fp,"%d %d",&mallocSize, &numItems);
  FeatureVector *fv;
  for(int i = 0; i < numItems; i++)
  {
    fv = new FeatureVector();
    unsigned numThreads;
    fscanf(fp, "%u %u %u", &(fv->addressStart), &(fv->rwSize), &numThreads);
    RWCounts *rwc;
    for(unsigned j = 0; j < numThreads; j++)
    {
      rwc = new RWCounts();
      unsigned threadId;
      fscanf(fp, "%u %u %u", &threadId, &(rwc->readCounts), &(rwc->writeCounts));
      fv->features[threadId]=rwc;
    }
    dataset.push_back(fv);
  }
  return dataset;
}
