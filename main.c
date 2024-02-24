#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <stdbool.h>

#include "ssbench.h"

static void usage(char *name)
{
  fprintf(stderr,
	  "USAGE: %s [-v] [-p port] [-w n,func]\n" \
	  "  demux messages from one or more ports to workers\n",
	  name);
}

struct Args Args = {
  .portCnt = 0,
  .workCnt = 0,
  .verbose = 0,
  .ssvrs   = NULL,
  .ssvrsSize = 0
};

static void dumpSSrvs()
{
  for (int i=0; i<Args.portCnt; i++) {
    fprintf(stderr, "Args.ssvrs[%d]=%p : ",i,Args.ssvrs[i]);
    sockserver_dump(Args.ssvrs[i], stderr);
  }
}

static void dumpArgs()
{
  fprintf(stderr, "Args.portCnt=%d Args.workCnt=%d\n"
	  "Args.ssvrs=%p, Args.ssvrs_size=%d\n" 
	  "Args.verbose=%d\n",
	  Args.portCnt,
	  Args.workCnt,
	  Args.ssvrs,
	  Args.ssvrsSize,
	  Args.verbose);
  dumpSSrvs();
  
}

static bool
processArgs(int argc, char **argv)
{
  char opt;
  while ((opt = getopt(argc, argv, "hp:w:v")) != -1) {
    switch (opt) {
    case 'h':
      usage(argv[0]);
      return false;
    case 'p':
      {
	int port;
	char *endptr;
	int i = Args.portCnt;
	
	port = strtod(optarg, &endptr);
	if (endptr == optarg) {
	  fprintf(stderr, "ERROR: invalid port value: %s\n",optarg);
	  usage(argv[0]);
	  return false;
	}
	if (Args.ssvrs == NULL) {
	  Args.ssvrsSize = 1;
	  Args.ssvrs = malloc(sizeof(sockserver_t)*Args.ssvrsSize);
	}
	if (i>=Args.ssvrsSize) {
	  Args.ssvrsSize = Args.ssvrsSize << 1;
	  Args.ssvrs=realloc(Args.ssvrs, sizeof(sockserver_t)*Args.ssvrsSize);
	}
	Args.ssvrs[i] = sockserver_new(port);
	Args.portCnt++;
      }
      break;
    case 'w':
      Args.workCnt++;
      break;
    case 'v':
      Args.verbose++;
      break;
    default:
      usage(argv[0]);
      return false;
    }
  } 

#if 0
  if ((argc - optind) < 3) {
    usage(argv[0]);
    return false;
  }

  Args.maskpath = argv[optind];
  Args.svpath = argv[optind+1];
  Args.reconspath = argv[optind+2];
#endif
  
  if (verbose(1)) dumpArgs();
  return true;
}

int main(int argc, char **argv)
{
  if (!processArgs(argc,argv)) return -1;

  VPRINT("%s: start: \n", argv[0]);
  return 0;
}

