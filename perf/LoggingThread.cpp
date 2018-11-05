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
    bufferS << "---" << hitMPerf->stop_and_collect() << "---\n";
    for (const auto &p: this->samples)
        bufferS << p << '\n';
//    bufferS << "===================================================\n"
//            << "Thread " << this->index << '\n';
//    cout << "Thread " << index << ", "
//         << "cache-events: " << hitMPerf->stop_and_collect() << "\n";
//         // << "load-events: " << loadStorePerf->stop_and_collect() << '\n';
//    typedef std::tuple<size_t, uint64_t, uint64_t> Rec;
//    vector<Rec> corr;
//    for (const auto &p: this->correlation)
//        corr.emplace_back(p.second, p.first.first, p.first.second);
//    sort(corr.begin(), corr.end(), [](const Rec &l, const Rec &r) {
//        return get<0>(l) > get<0>(r);
//    });
//    for (const auto &r: corr) {
//        size_t freq;
//        uint64_t from_addr, to_addr;
//        tie(freq, from_addr, to_addr) = r;
//        if (freq < Thread::totalCor * LeastFreq)
//            break;
//        bufferS << (void *) from_addr << "<->" << (void *) to_addr << ": " << freq << '('
//                << fixed << setprecision(2) << 100 * (double) freq / Thread::totalCor << "%)\n";
//    }
}
