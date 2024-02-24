#ifndef __SSBENCH_H__
#define __SSBENCH_H__

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "sockserver.h"
#include "worker.h"

struct Args {
  int portCnt;
  int workCnt;
  int verbose;
  sockserver_t *ssvrs;
  int ssvrsSize;
};

extern struct Args Args;
  
#define VLPRINT(VL, fmt, ...)  { if (verbose(VL)) { fprintf(stderr, "%s: " fmt, __func__, __VA_ARGS__); } }
#define VPRINT(fmt, ...) VLPRINT(1, fmt, __VA_ARGS__)

inline bool verbose(int l) { return Args.verbose >= l; }

#endif
