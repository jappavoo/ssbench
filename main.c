// _GNU_SOURCE is required to pickup getcpu
#define _GNU_SOURCE
#include "ssbench.h"
#include <errno.h>
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
  .inputCnt                = 0,
  .outputCnt               = 0,
  .funcCnt                 = 0,
  .verbose                 = 0,
  .totalcpus               = 0,
  .availcpus               = 0,
  .pid                     = 0,
  .inputServers.array     = NULL,
  .inputServers.arraySize = 0,
  .funcServers.hashtable     = NULL
};

static void dumpOutputSrvs()
{
}
static void dumpInputSrvs()
{
  for (int i=0; i<Args.inputCnt; i++) {
    fprintf(stderr, "Args.ssvrs[%d]=%p : ",i,
	    Args.inputServers.array[i]);
    sockserver_dump(Args.inputServers.array[i], stderr);
  }
}

static void dumpFuncSrvs()
{
}

static void dumpArgs()
{
  fprintf(stderr, "Args.pid=%d Args.totalcpus=%u, Args.availcpus=%u\n"
	  "Args.inputCnt=%d Args.outputCnt=%d\n Args.funcCnt=%d"
	  "Args.ssvrs=%p, Args.ssvrs_size=%d\n"
	  "Args.fsvrs=%p\n"
	  "Args.verbose=%d\n",
	  Args.pid, Args.totalcpus, Args.availcpus,
	  Args.inputCnt,
	  Args.outputCnt,
	  Args.funcCnt,
	  Args.inputServers.array,
	  Args.inputServers.arraySize,
	  Args.funcServers.hashtable,
	  Args.verbose);
  dumpInputSrvs();
  dumpOutputSrvs();
  dumpFuncSrvs();
}

static bool parseCPUSet(char *str, cpu_set_t * mask)
{
  int i=0;
  char *endptr;
  
  if (str) {
    while (*str != '\0') {
      errno = 0;
      int cpu = strtod(str, &endptr);
      if (endptr == str || errno != 0) {
	fprintf(stderr, "failed to parse cpu set should be a comma"
		" sperated list of cpus (%d)\n", errno);
	exit(-1);
      } else {
	if (cpu < Args.totalcpus) {
	  CPU_SET(cpu, mask);
	  i++;
	  VLPRINT(2,"%d added to mask:%p", cpu, mask);
	} else {
	  VLPRINT(0, "%d greater than totalcpus on the system: %d\n",
		  cpu, Args.totalcpus);
	}
      }
      if (*endptr != '\0') endptr++;
      str=endptr;
    }
  }
  if (i>0) return true;
  else return false;
}

static void
setAllcpus(int max, cpu_set_t *mask) {
  for (int i=0; i<max; i++) {
    CPU_SET(i,mask);
    VLPRINT(2, "%d added to mask:%p\n", i, mask);
  }
}

bool addOutput(char **argv, int argc, char *optarg)
{
  NYI;
  return false;
}

bool addInput(char **argv, int argc, char *optarg)
{
  int port;
  char *endptr;
  cpu_set_t cpumask;	
  int i = Args.inputCnt;
  
  errno = 0;   // as per man page notes for strtod or strtol
  port = strtod(optarg, &endptr);
  if (endptr == optarg || errno != 0) {
    fprintf(stderr, "ERROR: invalid port value: %s (%d)\n",optarg,errno);
    usage(argv[0]);
    return false;
  }
  
  CPU_ZERO(&cpumask); 
  if (*endptr != '\0') {
    // there is more to this argument assume it is a cpuset
    // specification.  Skip the non number character and parse
    endptr++;
    if (!parseCPUSet(endptr, &cpumask)) {
      fprintf(stderr, "ERROR: invalid cpu set specifiation: %s\n", optarg);
      usage(argv[0]);
      return false;
    }
  } else {
    // if no cpu mask specified then set mask to all cpus
    setAllcpus(Args.totalcpus, &cpumask);
  }
  
  if (Args.inputServers.array == NULL) {
    Args.inputServers.arraySize = 1;
    Args.inputServers.array =
      malloc(sizeof(sockserver_t)*Args.inputServers.arraySize);
  }
  if (i>=Args.inputServers.arraySize) {
    Args.inputServers.arraySize =
      Args.inputServers.arraySize << 1;
    Args.inputServers.array =
      realloc(Args.inputServers.array,
	      sizeof(sockserver_t)*Args.inputServers.arraySize);
  }
  Args.inputServers.array[i] = sockserver_new(port,i,cpumask);
  Args.inputCnt++;
}

bool addFunc(char **argv, int argc, char *optarg)
{
  uint32_t id;
  char *path;           // sscanf mallocs space for this see %ms
  funcserver_func *func;
  size_t maxmsgsize;
  size_t qlen;
  cpu_set_t cpumask;
  int n;
  n = sscanf(optarg, "%d,%ms,%lu,%lu", &id, &path, &maxmsgsize, &qlen);
  VLPRINT(2, "%d: %04x,%s,%lu,%lu\n", n, id, path, maxmsgsize, qlen);
  exit(-1);
}

static bool
processArgs(int argc, char **argv)
{
  char opt;

  unsigned int  cpu, node;

  Args.pid       = getpid();
  Args.totalcpus = get_nprocs_conf();
  Args.availcpus = get_nprocs();
  
  getcpu(&cpu, &node);

  while ((opt = getopt(argc, argv, "hi:f:o:v")) != -1) {
    switch (opt) {
    case 'f':
      if (!addFunc(argv, argc, optarg)) return false;
      break;
    case 'h':
      usage(argv[0]);
      return false;
    case 'i':
      if (!addInput(argv, argc, optarg)) return false;
      break;
    case 'o':
      if (!addOutput(argv, argc, optarg)) return false;
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
    
  if (Args.inputCnt==0) {
    VLPRINT(0, "ERROR:%d:Require at least one -i <inputport>[,cpu...]"
	    "input port server thread\n", Args.inputCnt);
    usage(argv[0]);
    return false;
  }

  pthread_barrier_init(&Args.funcServers.barrier, NULL,
		       Args.funcServers.num);

  pthread_barrier_init(&Args.inputServers.barrier, NULL,
		       Args.inputCnt);

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

  for (int i=1; i<Args.inputCnt; i++) {
    sockserver_start(Args.inputServers.array[i],
		     true // async
		     );
  }
  
  // run the 0th socketserver on the main thread
  sockserver_start(Args.inputServers.array[0],
		   false // async
		   );
  
  
  return 0;
}

