//
// Created by yifanz on 9/23/18.
//

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

#include "ProcMaps.h"

using namespace std;

struct MapsLine {
    uint64_t startAddr{}, endAddr{};
    string segName, perm;
};

MapsLine parseLine(string &curentry) {
    istringstream ss(curentry);
    MapsLine line;
    string inStr;
    ss >> inStr >> line.perm;
    size_t index = inStr.find('-');
    line.startAddr = stoul(inStr.substr(0, index), nullptr, 16);
    line.endAddr = stoul(inStr.substr(index + 1), nullptr, 16);
    ss >> inStr >> inStr >> inStr >> line.segName;
    return line;
}

vector<MapsLine> parseMapsFile() {
    ifstream iMapfile;
    string curentry;
    try {
        iMapfile.open("/proc/self/maps");
    } catch (...) {
        cerr << "can't open /proc/self/maps, exit now\n";
        abort();
    }

    vector<MapsLine> ret;
    while (getline(iMapfile, curentry)) {
        ret.emplace_back(parseLine(curentry));
    }
    return ret;
}

vector<MapsLine> filterBinaryRegions() {
    auto lines = parseMapsFile();
    size_t i = 1;
    for (; i < lines.size(); i++) {
        if (lines[i].segName != lines[i - 1].segName)
            break;
    }
    return vector<MapsLine>(lines.begin(), lines.begin() + i);
}

AddrSeg getTextRegion() {
    auto lines = filterBinaryRegions();
    for (auto &line : lines) {
        if (line.perm == "r-xp")
            return AddrSeg(line.startAddr, line.endAddr);
    }
    return AddrSeg(0, 0);  // info not found
}
