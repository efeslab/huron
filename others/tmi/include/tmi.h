#ifndef __AE_TMI_H__
#define __AE_TMI_H__

#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <linux/limits.h>
#include <sys/user.h>
#include "tramps.h"

#ifdef __cplusplus
extern "C" {
#endif

// For shared mem communication
typedef enum { SCH_EVT_SIGCHILD, 
               SCH_EVT_FORCE_FORK,
               SCH_EVT_NEW_THREAD,
               SCH_EVT_THREAD_EXIT,
               SCH_EVT_RESUME,
               SCH_EVT_EXIT} SCH_EVT_TYPE;

typedef struct sch_evt_t
{
    SCH_EVT_TYPE type;
    pid_t mpid, pid;
    void* data;
} sch_evt_t;

typedef struct thread_context
{
    void *(*start_routine)(void*);
    void *arg;
    int tid;
    int pipefd;
    INJECT_TRAMPOLINE* tramp;
} thread_context;

typedef struct tmi_shared_data
{
    int child_pipe_fd;
    void* child_epaddress;
} tmi_shared_data;

#define TMI_SHMEM_SIZE  sizeof(tmi_shared_data)
#define TMI_SHMEM_NAME  "TMI_SHMEM_NAME"

//#define PAGE_SIZE 4096
#define PAGE_BASE(v) ((void*)(((uintptr_t)(v)) & ~(PAGE_SIZE - 1)))

void die(const char* msg, ...);
#define MUST(cond) for(;;) { if(cond) break; die("%s failed at %s:%i (errno: %i)\n", #cond, __FILE__, __LINE__, errno); }

pid_t gettid( void );
void sch_write(int fd, SCH_EVT_TYPE type, pid_t mpid, pid_t pid, void* data);
int is_system_library(void* address);

#ifdef __cplusplus
}
#endif


#endif // __AE_TMI_H__
