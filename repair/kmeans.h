#include <map>
#include <vector>
#include <stdio.h>

using namespace std;

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

/*vector<unsigned> kmeans(unsigned k, vector<FeatureVector *> dataset)
{
}*/

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
