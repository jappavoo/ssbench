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
  .socketServers.array = NULL,
  .socketServers.arraySize = 0
};

static void dumpSSrvs()
{
  for (int i=0; i<Args.portCnt; i++) {
    fprintf(stderr, "Args.ssvrs[%d]=%p : ",i,
	    Args.socketServers.array[i]);
    sockserver_dump(Args.socketServers.array[i], stderr);
  }
}

static void dumpArgs()
{
  fprintf(stderr, "Args.portCnt=%d Args.workCnt=%d\n"
	  "Args.ssvrs=%p, Args.ssvrs_size=%d\n" 
	  "Args.verbose=%d\n",
	  Args.portCnt,
	  Args.workCnt,
	  Args.socketServers.array,
	  Args.socketServers.arraySize,
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
	if (Args.socketServers.array == NULL) {
	  Args.socketServers.arraySize = 1;
	  Args.socketServers.array =
	    malloc(sizeof(sockserver_t)*Args.socketServers.arraySize);
	}
	if (i>=Args.socketServers.arraySize) {
	  Args.socketServers.arraySize =
	    Args.socketServers.arraySize << 1;
	  Args.socketServers.array =
	    realloc(Args.socketServers.array,
		    sizeof(sockserver_t)*Args.socketServers.arraySize);
	}
	Args.socketServers.array[i] = sockserver_new(port,i);
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
    VLPRINT(0, "ERROR:%d:Require at least one -p <port> to a socket server"
	    "thread\n", Args.portCnt);
    usage(argv[0]);
    return false;
  }

  pthread_barrier_init(&Args.socketServers.barrier, NULL,
		       Args.portCnt);
  
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
    sockserver_start(Args.socketServers.array[i],
		     true // async
		     );
  }

  // run the 0th socketserver on the main thread
  sockserver_start(Args.socketServers.array[0],
		   false // async
		   );

  
  return 0;
}

