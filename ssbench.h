#ifndef __SSBENCH_H__
#define __SSBENCH_H__

#define _XOPEN_SOURCE 600 
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#define _XOPEN_SOURCE 600 
#include <pthread.h>

#include <sys/types.h>

#include "net.h"
#include "sockserver.h"
#include "msgserver.h"

struct Args {
  int portCnt;
  int workCnt;
  int verbose;
  sockserver_t *ssvrs;
  int ssvrsSize;
  pthread_barrier_t ssvrBarrier; 
};

extern struct Args Args;
  
#define VLPRINT(VL, fmt, ...)  { if (verbose(VL)) { fprintf(stderr, "%s: " fmt, __func__, __VA_ARGS__); } }
#define VPRINT(fmt, ...) VLPRINT(1, fmt, __VA_ARGS__)

inline bool verbose(int l) { return Args.verbose >= l; }

#endif
