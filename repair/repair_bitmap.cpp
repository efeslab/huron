#include <stdio.h>
#include <map>
#include <vector>

using namespace std;

int main(void)
{
  FILE *fp = fopen("record_output.log", "r");
  FILE *locFile = fopen("location.txt", "w");
  FILE *layoutFile = fopen("layout.txt", "w");
  int numOfMallocs, mallocId;
  int pcSize;
  int *pcFunctions;
  int *pcIndices;
  int mallocSize, numItems;
  fscanf(fp,"%d",&numOfMallocs);
  fprintf(locFile, "%d\n", numOfMallocs);
  fprintf(layoutFile, "%d\n", numOfMallocs);
  for(unsigned m = 0; m < numOfMallocs; m++) {
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
      int bitMask = 0;
      for(int j = 0; j < numThreads; j++){
        fscanf(fp, "%d", &threadId);
        bitMask |= 1<<threadId;
      }
      map<int, vector<int>>::iterator location = threadIds.find(bitMask);
      if (location == threadIds.end())
      {
        threadIds[bitMask] = vector<int>(); 
      }
      for(int k = 0; k < rwSize; k++)
      {
        threadIds[bitMask].push_back(index+k);
      }

    }
    int offset = 0;
    int mapSize = 0;
    unsigned maxOffset=0;
    map<int, int> redirectAddress;
    for(map<int, vector<int>>::iterator i = threadIds.begin(); i != threadIds.end(); i++)
    {
      //printf("%d\n",i->first);
      /*if(offset%64)
      {
        printf("%d\n",offset);
        offset += (64 - (offset%64));
        printf("%d\n",offset);
      }*/
      if(offset!=0)offset+=64;
      for(int j = 0; j < (i->second).size(); j++)
      {
        unsigned currentIndex = (i->second)[j];
        redirectAddress[currentIndex]=offset;
        if(currentIndex > maxOffset)maxOffset = currentIndex;
        offset += 1;
        mapSize += 1;
      }
    }
    fprintf(locFile,"%d\n%d\n", mallocId,pcSize);
    for(int i = 0; i < pcSize; i++)
    {
      fprintf(locFile,"%d %d\n",pcFunctions[i], pcIndices[i]);
    }
    fprintf(layoutFile, "%d\n",mallocId);
    fprintf(layoutFile, "%d\n",maxOffset+1);
    fprintf(layoutFile, "%d\n",offset);
    fprintf(layoutFile, "%d\n",mapSize);
    for (map<int,int>::iterator i = redirectAddress.begin(); i != redirectAddress.end(); i++)
    {
      fprintf(layoutFile,"%d %d\n", i->first, i->second);
    }
  }
  fclose(fp);
  fclose(locFile);
  fclose(layoutFile);
  return 0;
}
