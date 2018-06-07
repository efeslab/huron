#include <cstdlib>
#include <fstream>
#include <string>

// We may use a inode attribute to analyze whether we need to do this.
void getRegionInfo(std::string &curentry, void **start, void **end) {
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
    *start = (void *) strtoul(beginstr.c_str(), nullptr, 16);
    *end = (void *) strtoul(endstr.c_str(), nullptr, 16);

}

// Trying to get information about global regions.
void getGlobalRegion(uintptr_t *start, uintptr_t *end) {
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
    bool toExit = false;
    bool toSaveRegion = false;

    void *startaddr, *endaddr;
    string nextentry;
    int globalCount = 0;
    int prevRegionNumb = 0;

    while (getline(iMapfile, curentry)) {
        // Check the globals for the application. It is at the first entry
        if (((curentry.find(" rw-p ", 0) != string::npos) ||
             (curentry.find(" rwxp ", 0) != string::npos))) {
            // Now it is start of global of applications
            getRegionInfo(curentry, &startaddr, &endaddr);

            getline(iMapfile, nextentry);

            void *newstart;
            void *newend;
            if (nextentry.find("lib") == string::npos ||
                (nextentry.find(" rw-p ") != string::npos)) {
                getRegionInfo(nextentry, &newstart, &newend);
            }
            *start = (uintptr_t)startaddr;
            *end = (uintptr_t)(newstart == endaddr ? newend : endaddr);
            break;
        }
    }
    iMapfile.close();
}
