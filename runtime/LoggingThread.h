#ifndef LOGGINGTHREAD_H
#define LOGGINGTHREAD_H

#include <pthread.h>
#include <unordered_map>
#include "MallocInfo.h"

typedef void *threadFunction(void *);

const size_t LOG_SIZE = 1 << 16;

struct LocRecord {
    uintptr_t addr;
    uint16_t func_id, inst_id, size;

    LocRecord(uintptr_t _addr, uint16_t _func_id, uint16_t _inst_id, uint16_t _size) :
            addr(_addr), func_id(_func_id), inst_id(_inst_id), size(_size) {}

    LocRecord() = default;

    void dump(FILE *fd, int thread_fd, unsigned int r, unsigned int w) const {
        fprintf(fd, "%d,%p,%u,%u,%u,%u,%u\n", thread_fd, (void *) addr, func_id, inst_id, size, r, w);
    }

    bool operator == (const LocRecord &rhs) const {
        return (
                addr == rhs.addr &&
                func_id == rhs.func_id &&
                inst_id == rhs.inst_id
        );
    }
};

namespace std {
    template<>
    struct hash<LocRecord> {
        template<class T>
        inline void hash_combine(std::size_t &seed, const T &v) const {
            // From boost library.
            std::hash<T> hasher;
            seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        }

        std::size_t operator()(const LocRecord &k) const {
            using std::hash;
            size_t seed = 0;
            hash_combine(seed, k.addr);
            hash_combine(seed, k.func_id);
            hash_combine(seed, k.inst_id);
            hash_combine(seed, k.size);
            return seed;
        }
    };
}

struct Thread {
    // Buffer for read/write records.
    std::unordered_map<LocRecord, std::pair<unsigned int, unsigned int>> outputBuf;
    // File handle
    FILE *buffer_f;
    // Results of pthread_self
    pthread_t self;
    // The following is the parameter about starting function.
    threadFunction *startRoutine;
    void *startArg;
    // We used this to record the stack range
    // void * stackBottom;
    // void * stackTop;
    // index of this thread object.
    int index;
    // True: malloc will call our malloc_hook.
    bool malloc_hook_active;

    Thread(): buffer_f(nullptr), startRoutine(nullptr), startArg(nullptr), malloc_hook_active(false) {
        this->outputBuf.reserve(LOG_SIZE);
    }

    void flush_log();

    void log_load_store(const LocRecord &rw, bool is_write);

    std::string get_filename();

    void close_buffer();
};

#endif
