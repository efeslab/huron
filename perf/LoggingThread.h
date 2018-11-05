#ifndef LOGGINGTHREAD_H
#define LOGGINGTHREAD_H

#include <string>
#include <atomic>
#include <queue>
#include <unordered_map>
#include <fstream>
#include "Perf.h"

typedef void *threadFunction(void *);

namespace std {
    template<typename T1, typename T2>
    struct hash<std::pair<T1, T2>> {
        std::size_t operator()(const std::pair<T1, T2> &x) const {
            return std::hash<T1>()(x.first) ^ std::hash<T2>()(x.second);
        }
    };
}

struct Thread {
    static std::atomic<size_t> totalCor;
    // Perf object
    Perf *hitMPerf;
    std::vector<uint64_t> samples;
    // The following is the parameter of starting function.
    threadFunction *startRoutine;
    void *startArg;
    // index of this thread object.
    int index;
    // True: malloc & pthread_create are our version.
    bool all_hooks_active;
    std::atomic<bool> *writing;

    Thread(int _index, threadFunction _startRoutine, void *_startArg);

    void *run_self();

    void install_self();

    void remove_self();

    void put_list(std::ostream &bufferS);
};

extern __thread Thread *current;

class HookDeactivator {
    Thread *current_copy;
public:
    HookDeactivator() noexcept: current_copy(current) { if (current_copy) current_copy->all_hooks_active = false; }

    ~HookDeactivator() noexcept { if (current_copy) current_copy->all_hooks_active = true; }

};

#endif
