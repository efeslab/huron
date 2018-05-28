#include "LoggingThread.h"

// File name of the logging file.
const char log_file_name_template[] = "%d.log";

void Thread::flush_log() {
    if (!this->buffer_f) {
        auto filename = get_filename();
        this->buffer_f = fopen(filename.c_str(), "a");
        if (!this->buffer_f) {
            fprintf(stderr, "Cannot open file!!\n");//TODO: there is a problem here with main-0 thread
            //exit(1);
            return;
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

std::string Thread::get_filename() {
    return "__record__" + std::to_string(this->index) + ".log";
}
