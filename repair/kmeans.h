#include <map>
#include <vector>

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
  map<unsigned, RWCounts &> features;
};

vector<unsigned> kmeans(unsigned k, vector<FeatureVector &> dataset)
{
}

vector<FeatureVector &> readData(FILE *fp)
{
  vector<FeatureVector &> dataset;
  int mallocSize, numItems;
  fscanf(fp,"%d %d",&mallocSize, &numItems);
  for(int i = 0; i < numItems; i++)
  {
    int index, rwSize, numThreads, threadId;
    //
  }
}
