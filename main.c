// _GNU_SOURCE is required to pickup getcpu
#define _GNU_SOURCE
#include "ssbench.h"
#include <getopt.h>
#include <sys/sysinfo.h>

static void usage(char *name)
{
  fprintf(stderr,
	  "USAGE: %s [-v] [-p port] [-w n,func]\n" \
	  "  demux messages from one or more ports to workers\n",
	  name);
}

struct Args Args = {
  .portCnt                 = 0,
  .workCnt                 = 0,
  .verbose                 = 0,
  .totalcpus               = 0,
  .availcpus               = 0,
  .socketServers.array     = NULL,
  .socketServers.arraySize = 0,
  .opServers.hashtable     = NULL,
  .opServers.num           = 0
};

static void dumpSSrvs()
{
  for (int i=0; i<Args.portCnt; i++) {
    fprintf(stderr, "Args.ssvrs[%d]=%p : ",i,
	    Args.socketServers.array[i]);
    sockserver_dump(Args.socketServers.array[i], stderr);
  }
}

static void dumpOSrvs()
{
}

static void dumpArgs()
{
  fprintf(stderr, "Args.portCnt=%d Args.workCnt=%d\n"
	  "Args.ssvrs=%p, Args.ssvrs_size=%d\n"
	  "Args.osvrs=%p  Args.osvrs.num=%d\n"
	  "Args.totalcpus=%u, Args.availcpus=%u\n"
	  "Args.verbose=%d\n",
	  Args.portCnt,
	  Args.workCnt,
	  Args.socketServers.array,
	  Args.socketServers.arraySize,
	  Args.opServers.hashtable,
	  Args.opServers.num,
	  Args.totalcpus, Args.availcpus,
	  Args.verbose);
  dumpSSrvs();
  dumpOSrvs();
}

static bool parseCPUSet(char *, cpu_set_t * mask)
{
#if 0
  while (*endptr) {
    char *rest = endptr+1;
    if (*rest==0) break;
    int cpu = strtod(rest, &endptr);
    if (endptr != rest) fprintf(stderr, "cpu:%d\n", cpu);
    else break;
  }
#endif
  return false;
}

static bool
processArgs(int argc, char **argv)
{
  char opt;

  unsigned int  cpu, node;

  Args.totalcpus = get_nprocs_conf();
  Args.availcpus = get_nprocs();
  
  getcpu(&cpu, &node);

  while ((opt = getopt(argc, argv, "hp:w:v")) != -1) {
    switch (opt) {
    case 'h':
      usage(argv[0]);
      return false;
    case 'p':
      {
	int port;
	char *endptr;
	cpu_set_t cpumask;
	int i = Args.portCnt;
	
	port = strtod(optarg, &endptr);
	if (endptr == optarg) {
	  fprintf(stderr, "ERROR: invalid port value: %s\n",optarg);
	  usage(argv[0]);
	  return false;
	}
	if (*endptr) parseCPUSet(endptr, &cpumask);
	
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

  pthread_barrier_init(&Args.opServers.barrier, NULL,
		       Args.opServers.num);

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

