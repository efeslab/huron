#include <iomanip>
#include <iostream>
#include <algorithm>
#include "LoggingThread.h"

using namespace std;

extern AddrSeg text;

std::atomic<size_t> Thread::totalCor(0);

Thread::Thread(int _index, threadFunction _startRoutine, void *_startArg) :
        startRoutine(_startRoutine), startArg(_startArg),
        index(_index), all_hooks_active(false) {
    writing = new atomic<bool>(false);
}

void *Thread::run_self() {
    install_self();
    void *res = this->startRoutine ? this->startRoutine(this->startArg) : nullptr;
    remove_self();
    return res;
}

void Thread::install_self() {
    auto filter = [](const record_sample &sample) {
        return ::text.contain(sample.ip);
    };
    hitMPerf = new Perf(HitMEvent, filter, [this](const record_sample &sample) {
        this->samples.emplace_back(sample.ip);
    });
    *writing = true;
    hitMPerf->start();
    current = this;
    this->all_hooks_active = true;
}

void Thread::remove_self() {
    this->all_hooks_active = false;
}

void Thread::put_list(ostream &bufferS) {
    *writing = false;
    hitMPerf->stop_and_collect();
    for (const auto &p: this->samples)
        bufferS << p << '\n';
}
