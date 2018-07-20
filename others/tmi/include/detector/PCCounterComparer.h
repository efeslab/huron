#include <map>
#include <cstdint>

class PCCounterComparer
{
public:
	bool operator()(const std::pair<uint64_t, int>& first, const std::pair<uint64_t, int>& second) const
	{
		return first.second > second.second;
	}
};