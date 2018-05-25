#include "LoggingThread.h"
#include <cstdio>

// File name of the logging file.
const char log_file_name_template[] = "__record__%d.log";

void Thread::flush_log() {
    if (!this->buffer_f) {
        char filename[100];
        sprintf(filename, log_file_name_template, this->index);
        this->buffer_f = fopen(filename, "a");
        if (!this->buffer_f) {
            fprintf(stderr, "Cannot open file!!\n");//TODO: there is a problem here with main-0 thread
            //exit(1);
            return;
        }
    }
    for (int i = 0; i < this->output_n; i++) {
        const auto &rec = this->outputBuf[i];
        fprintf(this->buffer_f, "%d,%p,%d,%d,%d,%s\n", this->index, (void *)rec.addr,
                rec.func_id, rec.inst_id, rec.size, (rec.is_write ? "true" : "false"));
    }
    this->output_n = 0;
}

void Thread::log_load_store(uintptr_t addr, uint16_t func_id, uint16_t inst_id, uint16_t size,
                            bool is_write) {
    if (this->output_n == LOG_SIZE)
        this->flush_log();
    this->outputBuf[this->output_n++] = RWRecord(addr, func_id, inst_id, size, is_write);
}
