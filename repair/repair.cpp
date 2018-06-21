#include <stdio.h>
#include <map>
#include <vector>

using namespace std;


int main(void)
{
  FILE *fp = fopen("record_output.log", "r");
  int mallocSize;
  fscanf(fp, "%d", &mallocSize);
  int mallocId;
  int pcSize;
  int *pcFunctions;
  int *pcIndices;
  int mallocSize, numItems;
  fscanf(fp,"%d",&mallocId);
  fscanf(fp,"%d",&pcSize);
  pcFunctions = new int[pcSize];
  pcIndices = new int[pcSize];
  for(int i = 0; i < pcSize; i++)
  {
    fscanf(fp,"%d %d", &pcFunctions[i], &pcIndices[i]);
  }
  fscanf(fp,"%d %d",&mallocSize, &numItems);
  map<int, vector<int>> threadIds;
  for(int i = 0; i <numItems; i++)
  {
    int index, rwSize, numThreads, threadId;
    fscanf(fp,"%d %d %d",&index, &rwSize, &numThreads);
    for(int j = 0; j < numThreads; j++){
      fscanf(fp, "%d", &threadId);
      map<int, vector<int>>::iterator location = threadIds.find(threadId);
      if (location == threadIds.end())
      {
        threadIds[threadId] = vector<int>(); 
      }
      for(int k = 0; k < rwSize; k++)
      {
        threadIds[threadId].push_back(index+k);
      }
    }
  }
  fclose(fp);
  int offset = 0;
  map<int, int> redirectAddress;
  for(map<int, vector<int>>::iterator i = threadIds.begin(); i != threadIds.end(); i++)
  {
    //printf("%d\n",i->first);
    for(int j = 0; j < (i->second).size(); j++)
    {
      redirectAddress[(i->second)[j]]=offset;
      offset += 1;
    }
  }
  fp = fopen("location.txt", "w");
  //fprintf(fp,"%d\n%d\n", mallocId,pcSize);
  for(int i = 0; i < pcSize; i++)
  {
    fprintf(fp,"%d %d\n",pcFunctions[i], pcIndices[i]);
  }
  fclose(fp);
  fp = fopen("layout.txt", "w");
  fprintf(fp, "%d\n",mallocId);
  fprintf(fp,"%d\n",offset);
  for (map<int,int>::iterator i = redirectAddress.begin(); i != redirectAddress.end(); i++)
  {
    fprintf(fp,"%d %d\n", i->first, i->second);
  }
  fclose(fp);
  return 0;
}
