#ifndef LOGGINGTHREAD_H
#define LOGGINGTHREAD_H

#include <pthread.h>
#include <unordered_map>
#include <atomic>

typedef void *threadFunction(void *);

const size_t LOG_SIZE = 1 << 16;

struct LocRecord {
    uintptr_t addr;
    uint32_t func_id, inst_id;
    uint16_t size;
    bool is_heap;
    uint32_t m_id, m_offset;

    LocRecord(uintptr_t _addr, uint32_t _func_id, uint32_t _inst_id, uint16_t _size,
              uint32_t m_id, uint32_t m_size) :
            addr(_addr), func_id(_func_id), inst_id(_inst_id), size(_size),
            is_heap(true), m_id(m_id), m_offset(m_size) {}

    LocRecord(uintptr_t _addr, uint32_t _func_id, uint32_t _inst_id, uint16_t _size) :
            addr(_addr), func_id(_func_id), inst_id(_inst_id), size(_size),
            is_heap(false), m_id(), m_offset() {}

    LocRecord() = default;

    void dump(FILE *fd, int thread_fd, unsigned int r, unsigned int w) const {
        if (is_heap)
            fprintf(fd, "%d,%p,%u,%u,%u,%u,%u,%u,%u\n",
                thread_fd, (void *) addr, m_id, m_offset, func_id, inst_id, size, r, w);
        else
            fprintf(fd, "%d,%p,-1,-1,%u,%u,%u,%u,%u\n",
                    thread_fd, (void *) addr, func_id, inst_id, size, r, w);
    }

    bool operator==(const LocRecord &rhs) const {
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
            hash_combine(seed, k.is_heap);
            hash_combine(seed, k.m_id);
            hash_combine(seed, k.m_offset);
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
    // pthread_t self;
    // The following is the parameter about starting function.
    threadFunction *startRoutine;
    void *startArg;
    // True: thread is writing to its buffer.
    std::atomic<bool> *writing;
    // index of this thread object.
    int index;
    // True: malloc & pthread_create are our version.
    bool all_hooks_active;

    Thread(int _index, threadFunction _startRoutine, void *_startArg);

    void flush_log();

    void log_load_store(const LocRecord &rw, bool is_write);

    std::string get_filename();

    void open_buffer();

    void stop_logging();
};

extern __thread Thread *current;

class HookDeactivator {
    Thread *current_copy;
public:
    HookDeactivator() noexcept: current_copy(current) { current_copy->all_hooks_active = false; }

    ~HookDeactivator() noexcept { current_copy->all_hooks_active = true; }

    Thread *get_current() {
        return current_copy;
    }
};

#endif
