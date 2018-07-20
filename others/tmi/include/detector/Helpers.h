#pragma once
#include <cstdint>
#include <sstream>
#include <inttypes.h>
using namespace std;



#define DebugVerboseOutput false
#define DebugOutput(x) do{if(DebugVerboseOutput) x}while(false)

string uint64ToString(uint64_t value) {
	std::ostringstream os;
	os << value;
	return os.str();
}

string longToString(long value) {
	std::ostringstream os;
	os << value;
	return os.str();
}

string uint64ToHexString(uint64_t value)
{
	std::ostringstream os;
	os << "0x" << std::setfill('0') << std::setw(12) << std::hex << value << std::dec;
	return os.str();
}

template <class T>
inline std::string ToString(const T& t)
{
    std::stringstream ss;
    ss << t;
    return ss.str();
}

string execArbitraryShellCmd(const char* cmd)
{
#ifdef VISUAL_STUDIO
	FILE* pipe = _popen(cmd, "r");
#else
	FILE* pipe = popen(cmd, "r");
#endif
	if (!pipe) return "ERROR";
	char buffer[128];
	string result = "";
	while (!feof(pipe)) {
		if (fgets(buffer, 128, pipe) != NULL)
			result += buffer;
	}
#ifdef VISUAL_STUDIO
	_pclose(pipe);
#else
	pclose(pipe);
#endif
	return result;
}

uint64_t hexStringToUint64_t(string hexStr)
{
	uint64_t result = -1;
	stringstream S;
	S <<std::hex<<hexStr;
        S>>result;
	return result;
}

static vector<string> &split(const string &s, char delim, std::vector<string> &elems) {
	stringstream ss(s);
	string item;
	while (std::getline(ss, item, delim)) {
		elems.push_back(item);
	}
	return elems;
}


vector<string> split(const string &s, char delim) {
	std::vector<string> elems;
	split(s, delim, elems);
	return elems;
}
