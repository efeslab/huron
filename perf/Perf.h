//
// Created by yifanz on 9/12/18.
//

#ifndef RUNTIME_PERF_H
#define RUNTIME_PERF_H

#include <atomic>
#include <poll.h>
#include <thread>
#include <functional>
#include <linux/perf_event.h>
#include "Segment.h"

const size_t BufBits = 8, HitMEvent = 0x04d2, DefSamplePeriod = 1, DefWakeupPeriod = 50;

struct record_sample {
    struct perf_event_header header;
    uint64_t ip;         /* if PERF_SAMPLE_IP */
};

typedef std::function<bool(const record_sample &)> PredT;
typedef std::function<void(const record_sample &)> ActionT;

class Perf {
public:
    explicit Perf(size_t eventId, PredT pred, ActionT action,
                  uint64_t sample_period = DefSamplePeriod,
                  uint32_t wakeup_period = DefWakeupPeriod);

    ~Perf();

    void start();

    uint64_t stop_and_collect();

private:
    void async_poll_loop();

    void process_buffer();

    pollfd perf_poll{};
    PredT pred;
    ActionT action;
    perf_event_mmap_page *mpage{};
    std::thread *poll_thread{};
    char *buffer_base{};
    uint64_t last_offset{};
    uint64_t sample_counter{};
    uint32_t page_size{}, buf_size{};
    int fd{};
    std::atomic<bool> running;
};

#endif //RUNTIME_PERF_H
