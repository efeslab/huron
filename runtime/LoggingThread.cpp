#include "LoggingThread.h"

// File name of the logging file.
const char log_file_name_template[] = "__record__%d.log";

void Thread::flush_log() {
    if (!this->buffer_f) {
        char filename[100];
        sprintf(filename, log_file_name_template, this->index);
        this->buffer_f = fopen(filename, "a");
        if (!this->buffer_f) {
            fprintf(stderr, "Cannot open file!!\n");
            exit(1);
        }
    }
    for (int i = 0; i < this->output_n; i++) {
        this->outputBuf[i].dump(this->buffer_f, this->index);
    }
    this->output_n = 0;
}

void Thread::log_load_store(const RWRecord &rw) {
    if (this->output_n == LOG_SIZE)
        this->flush_log();
    this->outputBuf[this->output_n++] = rw;
}
