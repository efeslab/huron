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
    *start = (void *)strtoul(beginstr.c_str(), NULL, 16);
    *end = (void *)strtoul(endstr.c_str(), NULL, 16);

    return;
}

// Trying to get information about global regions.
void getGlobalRegion(void **start, void **end) {
    using namespace std;
    ifstream iMapfile;
    string curentry;

    //#define PAGE_ALIGN_DOWN(x) (((size_t) (x)) & ~xdefines::PAGE_SIZE_MASK)
    // void * globalstart = (void *)PAGE_ALIGN_DOWN(&__data_start);

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

    bool foundGlobals = false;

    while (getline(iMapfile, curentry)) {
        // Check the globals for the application. It is at the first entry
        if (((curentry.find(" rw-p ", 0) != string::npos) ||
             (curentry.find(" rwxp ", 0) != string::npos)) &&
            foundGlobals == false) {
            // Now it is start of global of applications
            getRegionInfo(curentry, &startaddr, &endaddr);

            // fprintf(stderr, "Initially, startaddr %p endaddr %p\n",
            // startaddr, endaddr);
            getline(iMapfile, nextentry);

            void *newstart;
            void *newend;
            // Check whether next entry should be also included or not.
            // if(nextentry.find("lib") == string::npos && (nextentry.find(" 08
            // ") != string::npos)) {
            if (nextentry.find("lib") == string::npos ||
                (nextentry.find(" rw-p ") != string::npos)) {
                //          fprintf(stderr, "Initially, nextentry.find %d
                //          string::npos %d\n", nextentry.find("lib"),
                //          string::npos); fprintf(stderr, "nextentry.now is
                //          %s\n", nextentry.c_str());
                getRegionInfo(nextentry, &newstart, &newend);
                //          fprintf(stderr, "Initially, startaddr %p endaddr
                //          %p\n", newstart, endaddr);
            }
            *start = startaddr;
            if (newstart == endaddr) {
                *end = newend;
            } else {
                *end = endaddr;
            }
            foundGlobals = true;
            break;
        }
    }
    iMapfile.close();
}
