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
#include <errno.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <fcntl.h>

#include "ssbench_types.h"

#include "ext/tsclog/cacheline.h"
#include "ext/tsclog/ntstore.h"
#include "ext/tsclog/now.h"
#include "ext/tsclog/buffer.h"
#include "ext/tsclog/tsclogc.h"

//#define ASSERTS_OFF
//#define VERBOSE_CHECKS_OFF

#ifdef ASSERTS_OFF
#define ASSERT(...)
#else
#define ASSERT(...) assert(__VA_ARGS__)
#endif

extern void cleanup();
#define EPRINT(fmt, ...) {fprintf(stderr, "%s: " fmt, __func__, __VA_ARGS__);}
static inline void EEXIT() {
  cleanup();
  exit(EXIT_FAILURE);
}

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

#include "net.h"
#include "msg.h"
#include "queue.h"
#include "func.h"
#include "worker.h"
#include "output.h"
#include "input.h"

struct Args {
  int          inputCnt;
  int          outputCnt;
  int          workerCnt;
  int          verbose;
  unsigned int totalcpus;
  unsigned int availcpus;
  pid_t        pid;
  
  struct {
    input_t           hashtable;
    pthread_barrier_t barrier;
  } inputs;

  struct {
    output_t          hashtable;
    pthread_barrier_t barrier;
  } outputs;
    
  struct {
    worker_t      hashtable;
    pthread_barrier_t barrier;
  } workers;
};

extern struct Args Args;

#ifdef VERBOSE_CHECKS_OFF
static inline bool verbose(int l) { return 0; }
#define VLPRINT(VL, fmt, ...)
#define VPRINT(fmt, ...)
#else
static inline bool verbose(int l) { return Args.verbose >= l; }
#define VLPRINT(VL, fmt, ...)  {					\
    if (verbose(VL)) {							\
	  fprintf(stderr, "%s: " fmt, __func__, __VA_ARGS__);		\
    } }

#define VPRINT(fmt, ...) VLPRINT(1, fmt, __VA_ARGS__)

#endif

#endif
