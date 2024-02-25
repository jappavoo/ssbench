#include "ssbench.h"
#include <getopt.h>

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

  if (verbose(1)) dumpArgs();
    
  if (Args.portCnt==0) {
    VLPRINT(0, "ERROR:%d:Require at least one -p <port> to a socket server thread\n",
	    Args.portCnt);
    usage(argv[0]);
    return false;
  }

  pthread_barrier_init(&Args.ssvrBarrier, NULL, Args.portCnt);
  
#if 0
  if ((argc - optind) < 3) {
    usage(argv[0]);
    return false;
  }

  Args.maskpath = argv[optind];
  Args.svpath = argv[optind+1];
  Args.reconspath = argv[optind+2];
#endif
  

  return true;
}

int main(int argc, char **argv)
{
  if (!processArgs(argc,argv)) return -1;

  for (int i=1; i<Args.portCnt; i++) {
    sockserver_start(Args.ssvrs[i],
		     true // async
		     );
  }

  // run the 0th socketserver on the main thread
  sockserver_start(Args.ssvrs[0],
		   false // async
		   );

  
  return 0;
}

