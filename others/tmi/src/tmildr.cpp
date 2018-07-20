#include <stdio.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>
#include <linux/unistd.h>
#include <linux/limits.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <errno.h>
#include <dirent.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/syscall.h>
#include <sys/ptrace.h>
#include <sys/stat.h>
#include <sys/user.h>
#include <linux/ptrace.h>
#include <set>
#include <vector>
#include <queue>
#include "tmi.h"

#include <chrono>
#include <iostream>

// XXX PARSE MAPS to make sure we exit library
#define TMISO_NAME "/home/delozier/tmi/build/lib/libtmiprotect.so"

typedef std::pair<pid_t,void*> context_pair;

std::vector<context_pair> all_contexts;
std::queue<pid_t> pids_to_fork;
std::vector<pid_t> pids_to_resume;
std::queue<struct user_regs_struct> regs_to_resume;

pid_t child_pid;

typedef struct data
{
  pid_t pre;
  pid_t pid;
  pid_t tid;
  int result;
} data;

enum { SCH_PIPE_RECV, SCH_PIPE_SEND, SCH_PIPE_COUNT };

static int sigchild_handler_pipes[SCH_PIPE_COUNT];

/////////////////////////////////////////////////////

static void sch_write(SCH_EVT_TYPE type, pid_t pid, void* data = NULL)
{
  sch_write(sigchild_handler_pipes[SCH_PIPE_SEND], type, getpid(), pid, data);
}

static void mknonblock(int fd)
{
  MUST(fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK) == 0);
}

#ifdef TMI_PROTECT
static void* find_tramp(pid_t pid)
{
  for(int c = 0; c < all_contexts.size(); c++){
    if(all_contexts[c].first == pid){
      return all_contexts[c].second;
    }
  }
  return nullptr;
}

typedef std::chrono::high_resolution_clock clock_type;

static void stop_the_world()
{
  printf("in stop the world\n");
  auto start = clock_type::now();

  for(int c = 0; c < all_contexts.size(); c++){
    pid_t tid = all_contexts[c].first;
    printf("Attempting to stop %d\n",tid);
    ptrace((__ptrace_request)PTRACE_ATTACH, tid, NULL, NULL);
    
    pids_to_fork.push(tid);

    /*
    int status = -1;
    do{
      waitpid(tid,&status,__WALL);
    }while(!WIFSTOPPED(status) || !WIFEXITED(status));
    */
  }
  auto end = clock_type::now();
  std::cout << "Stop the world: " << (end - start).count() << "\n";
}

static bool initiate_trampoline()
{
  if(pids_to_fork.empty()){
    return false;
  }

  auto start = clock_type::now();

  pid_t tid = pids_to_fork.front();
  pids_to_fork.pop();

  void *tramp_loc = find_tramp(tid);
  MUST(tramp_loc != nullptr);

  int status = -1;
  struct user_regs_struct regs;
  ptrace((__ptrace_request)PTRACE_GETREGS, tid, NULL, &regs);
  //printf("*** Target %i at RIP: %Lx\n", tid, regs.rip);
  printf("Stopped execution on %i at IP: %Lx\n",tid,regs.rip);

  /* TODO: Maybe use this to ensure we're not in something we haven't handled
  while(is_system_library((void*)regs.rip)){
    ptrace((__ptrace_request)PTRACE_SINGLE_STEP, tid, NULL, NULL);
    int status = -1;
    do{
      waitpid(tid,&status,__WALL);
    }while(!WIFSTOPPED(status));
    ptrace((__ptrace_request)PTRACE_GETREGS, tid, NULL, &regs);
  }

  printf("Actually stopped execution on %i at IP: %Lx\n",tid,regs.rip);
  */
  
  struct user_regs_struct orig_regs = regs;
  regs_to_resume.push(orig_regs);
  
  errno = 0;
  INJECT_TRAMPOLINE* tramp = (INJECT_TRAMPOLINE*)ptrace((__ptrace_request)PTRACE_PEEKTEXT, 
							tid, 
							tramp_loc,
							NULL);
  
  if(errno != 0){
    perror("Error: ");
  }
  
  MUST(errno == 0);
  printf("*** Installed trampoline for %i at %p\n", tid, tramp);
  regs.rip = (uintptr_t)tramp;
  ptrace((__ptrace_request)PTRACE_SETREGS, tid, NULL, &regs);
  ptrace((__ptrace_request)PTRACE_DETACH, tid, NULL, NULL);

  auto end = clock_type::now();
  std::cout << "Trampoline: " << (end - start).count() << "\n";

  return true;
}

static bool handle_evt_force_fork(sch_evt_t& evt)
{
  stop_the_world();
  initiate_trampoline();
    
  return true;
}

static bool handle_evt_resume(sch_evt_t& evt)
{
  auto start = clock_type::now();

  static int resumed = 0;
  ++resumed;
  pids_to_resume.push_back(evt.pid);
  pid_t new_child_pid = evt.pid;
  
  errno = 0;
  ptrace((__ptrace_request)PTRACE_ATTACH,new_child_pid,NULL,NULL);
  MUST(errno == 0);
  
  int status = -1;
  do{
    waitpid(new_child_pid,&status,__WALL);
  }while(!WIFSTOPPED(status));
  
  if(regs_to_resume.empty()){
    fprintf(stderr,"NO REGS TO RESUME!\n");
    return false;
  }
  
  struct user_regs_struct orig_regs = regs_to_resume.front();
  regs_to_resume.pop();
      
  // Restore registers
  errno = 0;
  ptrace((__ptrace_request)PTRACE_SETREGS, new_child_pid, NULL, &orig_regs);
  MUST(errno == 0);
  ptrace((__ptrace_request)PTRACE_DETACH, new_child_pid, NULL, NULL);
  MUST(errno == 0);

  auto end = clock_type::now();
  std::cout << "Resume: " << (end - start).count() << "\n";

  if(!pids_to_fork.empty()){
    initiate_trampoline();
  }

  return true;
}

static bool handle_evt_new_thread(sch_evt_t& evt)
{
  // printf("*** New thread created in: %i/%p\n", evt.pid, evt.data);
  all_contexts.push_back(context_pair(evt.pid,evt.data)); 
  return true;
}

static bool handle_evt_thread_exit(sch_evt_t& evt)
{
  int c = 0;
  printf("Trying to erase %d\n",evt.pid);
  for(c = 0; c < all_contexts.size(); c++){
    if(all_contexts[c].first == evt.pid){
      break;
    }
  }
  if(c < all_contexts.size()){
    printf("Erasing tid %d\n",evt.pid);
    all_contexts.erase(all_contexts.begin()+c);
  }
  return true;
}

static bool handle_loop_event(sch_evt_t& evt)
{
  switch(evt.type) {
  case SCH_EVT_FORCE_FORK:
    return handle_evt_force_fork(evt);
  case SCH_EVT_NEW_THREAD:
    return handle_evt_new_thread(evt);
  case SCH_EVT_THREAD_EXIT:
    return handle_evt_thread_exit(evt);
  case SCH_EVT_RESUME:
    return handle_evt_resume(evt);
  case SCH_EVT_EXIT:
    return false;
  }
  return true;
}

int loader_loop(pid_t child_first_pid)
{ 
  child_pid = child_first_pid;
  
  const int sigchild_fd = sigchild_handler_pipes[SCH_PIPE_RECV];

  while(1) {
    fd_set fds;
    int res;
    
    FD_ZERO(&fds);
    FD_SET(sigchild_fd, &fds);
    MUST((res = select(sigchild_fd + 1, &fds, NULL, NULL, NULL)) >= 0 || errno == EINTR);
    if(res > 0) {
      sch_evt_t evt;
      for(;;) {
	int amount = read(sigchild_fd, &evt, sizeof(evt));
	if(amount == -1 && errno == EWOULDBLOCK)
	  break;
	MUST(amount == sizeof(evt));
	
	bool keep_going = handle_loop_event(evt);
	if(!keep_going){
	  return 0;
	}
      }
    }
  }
  return 0;
}
#endif

void create_shmem()
{
  fprintf(stderr,"in create_shmem\n");

    char shmem_name[NAME_MAX];
    sprintf(shmem_name, "/tmi_%i_%i\n", getpid(), (int)time(NULL));
    setenv(TMI_SHMEM_NAME, shmem_name, 1);

    int shmfd = shm_open(shmem_name, O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR);
    MUST(shmfd >= 0);
    MUST(ftruncate(shmfd, TMI_SHMEM_SIZE) == 0);

    void* mem = mmap(NULL, TMI_SHMEM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shmfd, 0);
    MUST(mem != MAP_FAILED);

    tmi_shared_data* tsd = (tmi_shared_data*)mem;
    tsd->child_pipe_fd = sigchild_handler_pipes[SCH_PIPE_SEND];
}

static int run_child(int argc, char* argv[])
{
  for(int i = 1; i < argc; i++){
    argv[i-1] = argv[i];
  }
  argv[argc-1] = nullptr;

#ifdef TMI_PROTECT
    create_shmem();
#endif
    setenv("LD_PRELOAD", TMISO_NAME, 1);
    execvp(argv[0], argv);
    printf("execvp failed!\n");
    return -1;
}

int main(int argc, char* argv[])
{
    if(argc < 2)
    {
        printf("Syntax error: test5 <prog> [args...]\n");
        return -1;
    }
    

#ifdef TMI_PROTECT
    MUST(pipe(sigchild_handler_pipes) == 0);
    for(int i = 0; i < SCH_PIPE_COUNT; ++ i)
        mknonblock(sigchild_handler_pipes[i]);
#endif

#ifdef TMI_PROTECT
    pid_t child = fork();
    MUST(child != -1);
    if(child == 0)
        return run_child(argc, argv);

    return loader_loop(child);
#else
    return run_child(argc,argv);
#endif
}

