#pragma once
#define NumberOfTrackedEvents 4

enum SharingEventType : int
{
	TSWR = 0,
	TSWW = 1,
	FSWR = 2,
	FSWW = 3,
	Other = -1
};

std::string EventTypeToPrintableName(SharingEventType type)
{
	switch (type)
	{
	case TSWR:
		return "TSWR";
	case TSWW:
		return "TSWW";
		return "FSRW";
	case FSWR:
		return "FSWR";
	case FSWW:
		return "FSWW";
	case Other:
		return "Other";
	default:
		return "!INVALID!";
	}
}

int EventTypeToIndex(SharingEventType type)
{
	switch (type)
	{
	case SharingEventType::TSWR:
		return 0;
	case SharingEventType::TSWW:
		return 1;
	case SharingEventType::FSWR:
		return 2;
	case SharingEventType::FSWW:
		return 3;
	default:
		return -1;
	}
}

//might need to enable FSRWWR TSRWWR in the future. Retain this fast mapping.
SharingEventType IndexToEventType(int index)
{
	switch (index)
	{
	case 0:
		return SharingEventType::TSWR;
	case 1:
		return SharingEventType::TSWW;
	case 2:
		return SharingEventType::FSWR;
	case 3:
		return SharingEventType::FSWW;
	default:
		return SharingEventType::Other;
	}
}