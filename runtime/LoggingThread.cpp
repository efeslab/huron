#include "LoggingThread.h"
#include <cstdio>

// File name of the logging file.
const char *log_file_name = "__record__.log";

// File handler (to be init'ed) and file lock.
FILE *f;
std::mutex file_mut;

void Thread::flush_log() {
    file_mut.lock();
    if (!f)
        f = fopen(log_file_name, "a");
    for (int i = 0; i < this->output_n; i++) {
        const auto &rec = this->outputBuf[i];
        fprintf(f, "%d,%p,%d,%d,%d,%s\n", this->index, (void *)rec.addr,
                rec.func_id, rec.inst_id, rec.size, (rec.is_write ? "true" : "false"));
    }
    this->output_n = 0;
    file_mut.unlock();
}

void Thread::log_load_store(uintptr_t addr, uint16_t func_id, uint16_t inst_id, uint16_t size,
                            bool is_write) {
    if (this->output_n == LOG_SIZE)
        this->flush_log();
    this->outputBuf[this->output_n++] = RWRecord(addr, func_id, inst_id, size, is_write);
}
