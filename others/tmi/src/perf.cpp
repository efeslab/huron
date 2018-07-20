#include <linux/perf_event.h>
#include <asm/unistd.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/unistd.h>
#include <sys/ioctl.h>
#include <string.h>
#include <stdio.h>
#include "perf.hpp"

#define EXPECTED_SIZE (sizeof(perf_event_header) + 3 * sizeof(void*))
static inline void mb() { asm volatile ("":::"memory"); }

Perf::Perf(int sample_period) : _fd(-1), _lost(0), _done(false), _head(0), _tail(0)
{
    _page_size = sysconf(_SC_PAGESIZE);
    _buf_size = (1U << BUF_BITS) * _page_size;

    _sample_period = sample_period;

    memset(&_attr, 0, sizeof(_attr));
    _attr.type = PERF_TYPE_RAW;
    _attr.size = sizeof(_attr);
    _attr.sample_period = sample_period;
    _attr.sample_type = PERF_SAMPLE_ADDR | PERF_SAMPLE_IP | PERF_SAMPLE_TIME;
    //_attr.sample_type = PERF_SAMPLE_ADDR | PERF_SAMPLE_IP;
    _attr.exclude_kernel = 1;
    _attr.precise_ip = 2;
    _attr.config = EVENTID;
    _attr.disabled = 1;
}

Perf::~Perf()
{
    if (_mpage != (struct perf_event_mmap_page *)-1L)
        munmap(_mpage, _page_size + _buf_size);
    if (_fd != -1)
        close(_fd);
}

bool Perf::open(bool do_enable)
{
    _fd = syscall(__NR_perf_event_open, &_attr, 0, -1, -1, 0);
    if (_fd < 0){
      printf("Failed to get fd\n");
      return false;
    }
   
    _mpage = (struct perf_event_mmap_page*)mmap(NULL,  _page_size + _buf_size, PROT_READ|PROT_WRITE, MAP_SHARED, _fd, 0);

    fprintf(stderr,"Allocated perf page with size: %lu\n",_page_size + _buf_size);

    if (_mpage == (struct perf_event_mmap_page *)-1L){
      perror("Failed to mmap: ");
      return false;
    }
    _data = ((char*)_mpage) + _page_size;
    _data_end = _data + _buf_size;
    _buf_mask = _buf_size - 1;

    if(do_enable)
        return enable();

    return true;
}

bool Perf::enable()
{
    return ioctl(_fd, PERF_EVENT_IOC_ENABLE, 0) == 0;
}

bool Perf::disable()
{
    return ioctl(_fd, PERF_EVENT_IOC_DISABLE, 0) == 0;
}

bool Perf::is_done()
{
  return _done;
}

void Perf::finalize()
{
  _done = true;
}

void Perf::set_pid(int pid)
{
  _pid = pid;
}

int Perf::get_pid()
{
  return _pid;
}

bool Perf::start_iterate()
{
  _head = _mpage->data_head;
  _tail = _mpage->data_tail;
  mb();
  return true;
}
            
bool Perf::next(DataRecord* dr)
{
  while(_tail != _head)
    { 
      struct perf_event_header* hdr = (struct perf_event_header*)(_data + (_tail & _buf_mask));
      size_t size = hdr->size;
      _tail += size;
      
      if(hdr->type != PERF_RECORD_SAMPLE)
	continue;
      
      if(size != EXPECTED_SIZE)
	break;
      
      char* base = (char*)(hdr + 1);
      if(base >= _data_end) base = (char*)_data;
      dr->ip = *((void**)base);
      base += sizeof(uint64_t);
      
      if(base >= _data_end) base = (char*)_data;
      dr->time = *((uint64_t*)base);
      base += sizeof(void*);
      
      if(base >= _data_end) base = (char*)_data;
      dr->data = *((void**)base);
      base += sizeof(void**);

      dr->tid = _pid;
      
      return true;
    }
  abort_iterate();
  return false;
}   
    
void Perf::abort_iterate()
{
  _mpage->data_tail = _head;
  mb();
}



