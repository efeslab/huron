//
// Created by yifanz on 9/12/18.
//

#include <cstdio>
#include <system_error>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <cassert>
#include <utility>
#include "Perf.h"
#include "LoggingThread.h"

int perf_event_open(perf_event_attr *attr, pid_t pid, int cpu, int group_fd, unsigned long flags) {
    return (int) syscall(__NR_perf_event_open, attr, pid, cpu, group_fd, flags);
}

Perf::Perf(size_t eventId, PredT pred, ActionT action, uint64_t sample_period, uint32_t wakeup_period) :
        pred(std::move(pred)), action(std::move(action)), running(false) {
    page_size = (uint32_t) sysconf(_SC_PAGESIZE);
    buf_size = (1U << BufBits) * page_size;

    perf_event_attr _attr{};
    _attr.type = PERF_TYPE_RAW;
    _attr.size = sizeof(_attr);
    _attr.config = eventId;
    _attr.sample_period = sample_period;
    _attr.sample_type = PERF_SAMPLE_IP;
    _attr.exclude_kernel = 1;
    _attr.precise_ip = 2;
    _attr.disabled = 1;
    assert(wakeup_period);
    _attr.wakeup_events = wakeup_period;

    fd = perf_event_open(&_attr, (pid_t)syscall(__NR_gettid), 0, -1, 0);
    if (fd < 0)
        throw std::system_error(errno, std::system_category(), "Failed to get fd\n");

    mpage = (struct perf_event_mmap_page *) mmap(
            nullptr, page_size + buf_size, PROT_READ, MAP_SHARED, fd, 0
    );
    if (mpage == (struct perf_event_mmap_page *) -1L)
        throw std::system_error(errno, std::system_category(), "Failed to mmap");
    buffer_base = (char *) mpage + page_size;

    // Set poll struct, later poll() could be called upon it
    perf_poll.fd = fd;
    perf_poll.events = POLLIN;
}

Perf::~Perf() {
    if (mpage != (struct perf_event_mmap_page *) -1L)
        munmap(mpage, page_size + buf_size);
    if (fd != -1)
        close(fd);
}

void Perf::start() {
    HookDeactivator deactiv;
    if (ioctl(fd, PERF_EVENT_IOC_RESET, 0) != 0)
        throw std::system_error(errno, std::system_category(), "Cannot reset fd");
    if (ioctl(fd, PERF_EVENT_IOC_ENABLE, 0) != 0)
        throw std::system_error(errno, std::system_category(), "Cannot enable fd");
    running = true;
    poll_thread = new std::thread([this]() { async_poll_loop(); });
}

uint64_t Perf::stop_and_collect() {
    running = false;
    poll_thread->join();
    delete poll_thread;
    if (ioctl(fd, PERF_EVENT_IOC_DISABLE, 0) != 0)
        throw std::system_error(errno, std::system_category(), "Cannot disable fd");
    return sample_counter;
}

void Perf::async_poll_loop() {
    while (running) {
        int r = poll(&perf_poll, 1, 100);
        if (r < 0)
            throw std::system_error(errno, std::system_category(), "Failed to poll");
        else if (r == 0 && running) // timeout
            continue;
        else {
            process_buffer();
        }
    }
    process_buffer();
}

void Perf::process_buffer() {
    while (last_offset < mpage->data_head) {
        auto *sample = (record_sample *) (buffer_base + last_offset % buf_size);
        // allow only PERF_RECORD_SAMPLE
        if (sample->header.type == PERF_RECORD_SAMPLE)
            if (pred(*sample)) {
                sample_counter++;
                action(*sample);
            }
        if (mpage->data_head - last_offset > buf_size) {
            // fprintf(stderr, "Overflow\n");
            last_offset = mpage->data_head;
        } else
            last_offset += sample->header.size;
    }
}
