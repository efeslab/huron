#include <cstdlib>
#include <fstream>
#include <string>
#include <iostream>
#include "Segment.h"

// We may use a inode attribute to analyze whether we need to do this.
void getRegionInfo(std::string &curentry, uintptr_t *start, uintptr_t *end) {
    using namespace std;
    // Now this entry is about globals of libc.so.
    string::size_type pos = 0;
    string::size_type endpos = 0;
    // Get the starting address and end address of this entry.
    // Normally the entry will be like this
    // "00797000-00798000 rw-p ...."
    string beginstr, endstr;

    while (curentry[pos] != '-')
        pos++;

    beginstr = curentry.substr(0, pos);

    // Skip the '-' character
    pos++;
    endpos = pos;

    // Now pos should point to a space or '\t'.
    while (!isspace(curentry[pos]))
        pos++;
    endstr = curentry.substr(endpos, pos - endpos);

    // Save this entry to the passed regions.
    *start = strtoul(beginstr.c_str(), nullptr, 16);
    *end = strtoul(endstr.c_str(), nullptr, 16);

}

// Trying to get information about global regions.
AddrSeg getGlobalRegion() {
    using namespace std;
    ifstream iMapfile;
    string curentry;

    try {
        iMapfile.open("/proc/self/maps");
    } catch (...) {
        fprintf(stderr, "can't open /proc/self/maps, exit now\n");
        abort();
    }

    // Now we analyze each line of this maps file.
    uintptr_t startaddr, endaddr;
    string nextentry;

    while (getline(iMapfile, curentry)) {
        // Check the globals for the application. It is at the first entry
        if (((curentry.find(" rw-p ", 0) != string::npos) ||
             (curentry.find(" rwxp ", 0) != string::npos))) {
            // Now it is start of global of applications
            getRegionInfo(curentry, &startaddr, &endaddr);

            getline(iMapfile, nextentry);
            uintptr_t newstart, newend;
            if (nextentry.find("lib") != string::npos && (nextentry.find(" rw-p ") == string::npos))
                return AddrSeg(0, 0);
            getRegionInfo(nextentry, &newstart, &newend);
            return AddrSeg(startaddr, (newstart == endaddr ? newend : endaddr));
        }
    }
    return AddrSeg(0, 0);  // info not found
}
