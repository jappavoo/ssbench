#ifndef __SSBENCH_H__
#define __SSBENCH_H__

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>

#include <uthash.h>
#include "ext/tsclog/cacheline.h"
#include "ext/tsclog/ntstore.h"
#include "ext/tsclog/now.h"
#include "ext/tsclog/buffer.h"
#include "ext/tsclog/tsclogc.h"
#include "net.h"
#include "msg.h"
#include "queue.h"
#include "sockserver.h"
#include "opserver.h"

struct Args {
  int portCnt;
  int workCnt;
  int verbose;
  struct {
    sockserver_t *array;
    int arraySize;
    pthread_barrier_t barrier;
  } socketServers;
};

extern struct Args Args;
  
#define VLPRINT(VL, fmt, ...)  {		\
    if (verbose(VL)) {							\
	  fprintf(stderr, "%s: " fmt, __func__, __VA_ARGS__);		\
    } }
#define VPRINT(fmt, ...) VLPRINT(1, fmt, __VA_ARGS__)

static inline bool verbose(int l) { return Args.verbose >= l; }

#define NYI { fprintf(stderr, "%s: %d: NYI\n", __func__, __LINE__); } 
#endif
