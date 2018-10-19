#include "LoggingThread.h"

static inline uint64_t rdtscp (uint32_t &aux)
{
    uint64_t rax, rdx;
    asm volatile ( "rdtscp\n" : "=a" (rax), "=d" (rdx), "=c" (aux) : : );
    return (rdx << 32 ) + rax;
}

void Thread::flush_log() {
    uint64_t time;
    uint32_t time_aux;
    time = rdtscp(time_aux);
    for (const auto &rw_n: this->outputBuf)
        // if (rw_n.second.second)  // if is not read-only
        rw_n.first.dump(this->buffer_f, this->index, rw_n.second.first, rw_n.second.second, time);
    this->outputBuf.clear();
    this->access_count_in_buffer = 0;
}

void Thread::log_load_store(const LocRecord &rw, bool is_write) {
    if (!writing)
        return;
    if (this->outputBuf.size() == LOG_SIZE || this->access_count_in_buffer == ACCESS_THRESHOLD)
    {
        this->flush_log();
    }
    this->access_count_in_buffer += 1;
    auto it = this->outputBuf.find(rw);
    if (it != this->outputBuf.end()) {
        if (is_write)
            it->second.second++;
        else
            it->second.first++;
    } else {
        auto pair = is_write ? std::make_pair(0, 1) : std::make_pair(1, 0);
        this->outputBuf.emplace(rw, pair);
    }
}

std::string Thread::get_filename() {
    return "__record__" + std::to_string(this->index) + ".log";
}

void Thread::stop_logging() {
    *writing = false;
    flush_log();
    if (this->buffer_f)
        fclose(this->buffer_f);
}

void Thread::open_buffer() {
    auto filename = get_filename();
    this->buffer_f = fopen(filename.c_str(), "a");
    if (!this->buffer_f) {
        fprintf(stderr, "Cannot open file!!\n");
        return;
    }
    *writing = true;
}

Thread::Thread(int _index, threadFunction _startRoutine, void *_startArg) :
        buffer_f(nullptr), startRoutine(_startRoutine), startArg(_startArg),
        index(_index), all_hooks_active(false) {
    writing = new std::atomic<bool>;
    this->outputBuf.reserve(LOG_SIZE);
    this->open_buffer();
    this->access_count_in_buffer = 0;
}
