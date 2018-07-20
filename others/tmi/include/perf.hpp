#ifndef __AE_PERF_HPP__
#define __AE_PERF_HPP__

#include <linux/perf_event.h>
#include <stdint.h>

#define BUF_BITS 8
#define SAMPLE_PERIOD_DEFAULT 1
#define WINDOW_SIZE_DEFAULT 1000000
#define EVENTID 0x4D2

struct DataRecord
{
    uint64_t time;
    void* ip;
    void* data;
    int tid;
};

class Perf
{
public:
    Perf(int sample_period);
    ~Perf();

public:
    bool open(bool enable = true);
    bool enable();
    bool disable();
    void finalize();
    bool is_done();

public:
    bool start_iterate();
    bool next(DataRecord* dr);
    void abort_iterate();

    void set_pid(int pid);
    int get_pid();
 
private:
    friend class PerfIterator;

    int _pid;
    unsigned int _page_size, _buf_size, _buf_mask;
    int _sample_period;
    struct perf_event_attr _attr;
    struct perf_event_mmap_page *_mpage;
    char *_data, *_data_end;
    int _fd;

    int _lost;

    bool _done;

    uint64_t _head, _tail;
};


#endif // __AE_PERF_HPP__
