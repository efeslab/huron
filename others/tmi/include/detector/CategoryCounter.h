#pragma once
#include "SharingEventType.h"

class CategoryCounter
{
private:
	int Counters[NumberOfTrackedEvents];
public:
	CategoryCounter();
	void Increment(SharingEventType type, int amount);
	int GetCount(SharingEventType type);
	SharingEventType DominantType();
	CategoryCounter Merge(CategoryCounter counter);
};

CategoryCounter::CategoryCounter()
{
	for (int i = 0; i < NumberOfTrackedEvents; i++)
	{
		Counters[i] = 0;
	}
}

void CategoryCounter::Increment(SharingEventType type, int amount)
{
	Counters[EventTypeToIndex(type)]+=amount;
}

int CategoryCounter::GetCount(SharingEventType type)
{
	return Counters[EventTypeToIndex(type)];
}

//break ties arbitrarily
SharingEventType CategoryCounter::DominantType()
{
	int maxIdx = 0;
	int maxCnt = Counters[maxIdx];
	for (int i = 1; i < NumberOfTrackedEvents; i++)
	{
		if (Counters[i]>maxCnt)
		{
			maxCnt = Counters[i];
			maxIdx = i;
		}
	}
	if (maxCnt == 0) return SharingEventType::Other;
	return IndexToEventType(maxIdx);
}

CategoryCounter CategoryCounter::Merge(CategoryCounter other)
{
	CategoryCounter newCounter;
	for (int i = 0; i < NumberOfTrackedEvents; i++)
	{
		newCounter.Counters[i] = Counters[i] + other.Counters[i];
	}
	return newCounter;
}
