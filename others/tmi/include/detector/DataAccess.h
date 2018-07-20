#include "SharingEventType.h"
#include <cstdint>
#include <stdio.h>

//Memory event types
#define MemOpRead 'R'
#define MemOpWrite 'W'


//Cache and Cache address manipulations
#define CacheLineSize 64
#define CacheCapacity 8 //In megabytes
#define CacheAssociativity 8
#define CacheSetsNumber (CacheCapacity*1024/(CacheAssociativity*CacheLineSize)) //Check this. Forgot.
#define CacheLineOffsetMask (CacheLineSize-1)


class DataAccess
{
public:
  //the instruction pointer to an access operation that caused this data access.
  uint64_t PC;
  //accessed address
  uint64_t Address;
  //offset in the line 
  short Size;
  //either read or write
  char AccessType;

  //Two accesses overlaps iff:
  //*They access the same line
  //*Their accessed contents overlap.
  static bool IsDataAccessOverlapped(DataAccess da1, DataAccess da2);
  static SharingEventType DetermineEventType(DataAccess earlierAccess, DataAccess laterAccess);
  
  long LineAddress(uint64_t Address)
  {
    return (Address & (~CacheLineOffsetMask));
  }
  
  short Offset()
  {
    return Address & (CacheLineOffsetMask);
  }
};

bool DataAccess::IsDataAccessOverlapped(DataAccess da1, DataAccess da2)
{
	if (da1.LineAddress(da1.Address) != da2.LineAddress(da2.Address))
	{
		return false;
	}

	return da1.Offset() >= da2.Offset() ?
		da2.Offset() + da2.Size - 1 >= da1.Offset():
		da1.Offset() + da1.Size - 1 >= da2.Offset();
}

SharingEventType DataAccess::DetermineEventType(DataAccess earlierAccess, DataAccess laterAccess)
{
	if (DataAccess::IsDataAccessOverlapped(earlierAccess, laterAccess))
	{
		if (earlierAccess.AccessType == MemOpRead && laterAccess.AccessType == MemOpWrite)
		{
			return SharingEventType::TSWR;
		}
		else if (earlierAccess.AccessType == MemOpWrite && laterAccess.AccessType == MemOpRead)
		{
			return SharingEventType::TSWR;
		}
		else if (earlierAccess.AccessType == MemOpWrite && laterAccess.AccessType == MemOpWrite)
		{
			return SharingEventType::TSWW;
		}
		else
		{
		    return SharingEventType::TSWR;
		}
	}
	else
	{
		if (earlierAccess.AccessType == MemOpRead && laterAccess.AccessType == MemOpWrite)
		{
			return SharingEventType::FSWR;
		}
		else if (earlierAccess.AccessType == MemOpWrite && laterAccess.AccessType == MemOpRead)
		{
			return SharingEventType::FSWR;
		}
		else if (earlierAccess.AccessType == MemOpWrite && laterAccess.AccessType == MemOpWrite)
		{
			return SharingEventType::FSWW;
		}
		else 
		{
		    return SharingEventType::FSWR;
		}
	}
}

