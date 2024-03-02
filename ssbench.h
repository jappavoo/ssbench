#ifndef __SSBENCH_H__
#define __SSBENCH_H__

// required for sched.h to providle cpu macros
#define _GNU_SOURCE

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <sched.h>
#include <uthash.h>

#include "ssbench_types.h"

#include "ext/tsclog/cacheline.h"
#include "ext/tsclog/ntstore.h"
#include "ext/tsclog/now.h"
#include "ext/tsclog/buffer.h"
#include "ext/tsclog/tsclogc.h"
#include "net.h"
#include "msg.h"
#include "queue.h"
#include "func.h"
#include "sockserver.h"
#include "funcserver.h"

struct Args {
  int          inputCnt;
  int          outputCnt;
  int          funcCnt;
  int          verbose;
  unsigned int totalcpus;
  unsigned int availcpus;
  pid_t        pid;
  
  struct {
    sockserver_t      hashtable;
    pthread_barrier_t barrier;
  } inputServers;
  
  struct {
    funcserver_t      hashtable;
    pthread_barrier_t barrier;
  } funcServers;
};

extern struct Args Args;
  
#define VLPRINT(VL, fmt, ...)  {		\
    if (verbose(VL)) {							\
	  fprintf(stderr, "%s: " fmt, __func__, __VA_ARGS__);		\
    } }
#define VPRINT(fmt, ...) VLPRINT(1, fmt, __VA_ARGS__)

static inline bool verbose(int l) { return Args.verbose >= l; }

#define NYI { fprintf(stderr, "%s: %d: NYI\n", __func__, __LINE__); }

// if gettid optimization avaiable use it
#if __GLIBC_PREREQ(2,30)
#define _GNU_SOURCE
#include <unistd.h>
#else
#include <sys/syscall.h>
pid_t
gettid(void)
{
    return syscall(SYS_gettid);
}
#endif

static inline void
cpusetDump(FILE *file, cpu_set_t * cpumask)
{
  for (int j = 0; j < CPU_SETSIZE; j++) {
    if (CPU_ISSET(j, cpumask)) {
      fprintf(file, "%d ", j);
    }
  }
  fprintf(file, "\n");
}
  
#endif
