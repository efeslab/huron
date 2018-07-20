#include <cstdint>
#include <map>
#include <climits>
#include "SharingEventType.h"
#define NumberOfHotSpotPCsToTrack 8000
class HotSpotWindow
{
private:
	int eventCounters[NumberOfTrackedEvents];
public:
	uint64_t WindowStartCycle;
	uint64_t WindowLengthInCycles;
	std::pair<uint64_t, int> TopPCs[NumberOfHotSpotPCsToTrack];
	HotSpotWindow();
	bool WindowReady();
	void IncrementEventCount(SharingEventType eventType);
	void Clear();
	int GetEventCount(SharingEventType eventType);
	uint64_t GetTotalEventCount();
	static int GetSmallestWindowIndex(HotSpotWindow* windows, int windowCount, SharingEventType eventType);

};

HotSpotWindow::HotSpotWindow()
{
	Clear();
}

bool HotSpotWindow::WindowReady()
{
	return WindowLengthInCycles != 0;
}

void HotSpotWindow::IncrementEventCount(SharingEventType eventType)
{
	eventCounters[EventTypeToIndex(eventType)]++;
}

void HotSpotWindow::Clear()
{
	WindowLengthInCycles = 0;
	WindowStartCycle = 0;
	for (int i = 0; i < NumberOfTrackedEvents; i++)
	{
		eventCounters[i] = 0;
	}
	for (int i = 0; i < NumberOfHotSpotPCsToTrack; i++)
	{
		TopPCs[i].first = TopPCs[i].second = 0;
	}
}

int HotSpotWindow::GetEventCount(SharingEventType eventType)
{
	return eventCounters[EventTypeToIndex(eventType)];
}

uint64_t HotSpotWindow::GetTotalEventCount()
{
	uint64_t sum = 0;
	for (int i = 0; i < NumberOfTrackedEvents; i++)
	{
		sum += eventCounters[i];
	}
	return sum;
}

int HotSpotWindow::GetSmallestWindowIndex(HotSpotWindow* windows, int windowCount, SharingEventType eventType)
{
	int currMinCnt = INT_MAX;
	int currMinIndex = -1;
	int selectedIndex = EventTypeToIndex(eventType);
	for (int i = 0; i < windowCount; i++)
	{
		if (!windows[i].WindowReady()) continue;
		if (windows[i].eventCounters[selectedIndex] < currMinCnt)
		{
			currMinCnt = windows[i].eventCounters[selectedIndex];
			currMinIndex = i;
		}
	}
	return currMinIndex;
}
