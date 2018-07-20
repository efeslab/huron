#include <inttypes.h>
#include <iostream>
#ifndef Helpers
#define Helpers
#include "Helpers.h"
#endif
using namespace std;

#define MemRangeAttribute char
#define HeapFlag 1
#define StackFlag 2
#define DefiningApp 4


#define Permission char
#define RangeCanRead 1
#define RangeCanWrite 2
#define RangeCanExecute 4

class MemRangeBucket
{
public:
	uint64_t LowerBoundInclusive;
	uint64_t UpperBoundExclusive;
	uint64_t GetHitCount();
	string GetResidingLibrary();
	void IncrementCount();
	void Initialize(string lowerBoundInclusive, string upperBoundExclusive, string definingLib, MemRangeAttribute flags, Permission permission);
	MemRangeBucket();
	bool IsStackRange();
	bool IsHeapRange();
	bool IsInAppRange();
	bool HasPermission(Permission permission);
private:
	uint64_t hitCount;
	string residingLibrary;
	MemRangeAttribute flags;
	Permission permissions;
};

void MemRangeBucket::Initialize(string lowerBoundInclusive, string upperBoundExclusive, string definingLib, MemRangeAttribute attributes, Permission permission)
{
	LowerBoundInclusive = hexStringToUint64_t(lowerBoundInclusive);
	UpperBoundExclusive = hexStringToUint64_t(upperBoundExclusive);
	residingLibrary = definingLib;
	hitCount = 0;
	flags = attributes;
	permissions = permission;
}

MemRangeBucket::MemRangeBucket()
{
}

uint64_t MemRangeBucket::GetHitCount()
{
	return hitCount;
}

void MemRangeBucket::IncrementCount()
{
	hitCount++;
}

bool MemRangeBucket::IsHeapRange()
{
	return (flags & HeapFlag) != 0;
}

bool MemRangeBucket::IsStackRange()
{
	return (flags & StackFlag) != 0;
}

bool MemRangeBucket::IsInAppRange()
{
	return (flags & DefiningApp) != 0;
}

bool MemRangeBucket::HasPermission(Permission permission)
{
	return (permission & permissions) == permission;
}

string MemRangeBucket::GetResidingLibrary()
{
	return residingLibrary;
}
