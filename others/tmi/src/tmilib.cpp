#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/syscall.h>
#include <errno.h>
#include "tmi.h"

extern "C"{
void die(const char* msg, ...)
{
    va_list ap;
    va_start(ap, msg);
    vfprintf(stderr, msg, ap);
    exit(-1);
}

pid_t gettid( void ) 
{ 
    return syscall( __NR_gettid ); 
} 

void sch_write(int fd, SCH_EVT_TYPE type, pid_t mpid, pid_t pid, void* data)
{
    int old_errno;
    sch_evt_t evt;
    evt.type = type;
    evt.mpid = mpid;
    evt.pid = pid;
    evt.data = data;
    old_errno = errno;
    MUST(write(fd, &evt, sizeof(evt)) == sizeof(evt));
    errno = old_errno;
}

#define XXX_LIBRARY_CUTOFF 0x100000000000
int is_system_library(void* address)
{
    // XXX: should be fixed to a decent impl)
    return ((uintptr_t)address) >= XXX_LIBRARY_CUTOFF;
}
}
