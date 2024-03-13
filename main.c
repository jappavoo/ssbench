// _GNU_SOURCE is required to pickup getcpu
#define _GNU_SOURCE
#include "ssbench.h"
#include <errno.h>
#include <getopt.h>
#include <sys/sysinfo.h>
//012345678901234567890123456789012345678901234567890123456789012345678901234567
static void usage(char *name)
{
  fprintf(stderr,
	  "USAGE: %s [-v] [-i port[,cpu[,cpu,...]] "
	  "[-f id,func,maxmsgsize,qlen[,cpu[,cpu...]] "
	  "-o addr:port,maxmsgsize,qlen[,cpu[,cpu...]]\n"		
	  "  demux messages from one or more input ports to a specified "
	  "function and can establishes output connections for the functions"
	  " to send messages to.\n",
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
  .inputServers.hashtable  = NULL,
  .funcServers.hashtable   = NULL
  
};

static void dumpOutputSrvs()
{	  
}

static void dumpInputSrvs()
{
  sockserver_t ssrv, tmp;

  fprintf(stderr, "Args.inputServers.hashtable:\n");
  HASH_ITER(hh, Args.inputServers.hashtable, ssrv, tmp) {
    sockserver_dump(ssrv, stderr);
  }
}


static void dumpFuncSrvs()
{
  funcserver_t  fsrv, tmp;

  fprintf(stderr, "Args.funcServer.hashtable:\n");
  HASH_ITER(hh, Args.funcServers.hashtable, fsrv, tmp) {
    funcserver_dump(fsrv, stderr);
  }
}

static void dumpArgs()
{
  fprintf(stderr, "Args.pid=%d Args.totalcpus=%u, Args.availcpus=%u\n"
	  "Args.inputCnt=%d Args.outputCnt=%d Args.funcCnt=%d\n"
	  "Args.ssvrs=%p, Args.fsvrs=%p\n"
	  "Args.verbose=%d\n",
	  Args.pid, Args.totalcpus, Args.availcpus,
	  Args.inputCnt,
	  Args.outputCnt,
	  Args.funcCnt,
	  Args.inputServers.hashtable,
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
  int id = Args.inputCnt;
  sockserver_t ssrv;
  
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

  // given the id's are generated by us there should be no way
  // that an input server with this id should be in the hashtable
  HASH_FIND_INT(Args.inputServers.hashtable, &id, ssrv);
  assert(ssrv==NULL);
  ssrv = sockserver_new(port,id,cpumask);
  HASH_ADD_INT(Args.inputServers.hashtable, id, ssrv);   
  Args.inputCnt++;
  return true;
}

bool addFunc(char **argv, int argc, char *optarg)
{
  char *sptr;
  char *endptr;
  char *tmpstr;
  funcserver_t fsrv;
  
  uint32_t id;
  tmpstr = strtok_r(optarg, ",", &sptr);
  if (tmpstr == NULL) return false;
  errno = 0;   // as per man page notes for strtod or strtol
  id = strtod(tmpstr, &endptr);
  if (endptr == tmpstr || errno != 0) return false;
  /* id already in the hash? */
  HASH_FIND_INT(Args.funcServers.hashtable, &id, fsrv); 
  if (fsrv != NULL) {
    VLPRINT(0, "ERROR: %08x already defined as a function id\n", id);
    funcserver_dump(fsrv,stderr);
    return false;
  }

  ssbench_func_t func = NULL;
  char *path = strtok_r(NULL, ",", &sptr);
  if (path == NULL) return false;
  func = func_getfunc(path);
  if (func == NULL) {
    VLPRINT(0, "WARNING: no function associated with %s. Will use func=%p\n",
	    path, func);
  }

  size_t maxmsgsize;
  tmpstr = strtok_r(NULL, ",", &sptr);
  if (tmpstr == NULL) return false;
  errno = 0;   // as per man page notes for strtod or strtol
  maxmsgsize = strtod(tmpstr, &endptr);
  if (endptr == tmpstr || errno != 0) return false;
  
  size_t qlen;
  tmpstr = strtok_r(NULL, ",", &sptr);
  if (tmpstr == NULL) return false;
  errno = 0;   // as per man page notes for strtod or strtol
  qlen = strtod(tmpstr, &endptr);
  if (endptr == tmpstr || errno != 0) return false;

  cpu_set_t cpumask;
  CPU_ZERO(&cpumask);
  tmpstr = strtok_r(NULL, ",", &sptr);
  if (tmpstr != NULL) {
    if (!parseCPUSet(endptr, &cpumask)) {
      fprintf(stderr, "ERROR: invalid cpu set specifiation: %s\n", optarg);
      usage(argv[0]);
      return false;
    }
  } else {
    // if no cpu mask specified then set mask to all cpus
    setAllcpus(Args.totalcpus, &cpumask);
  }
  
  VLPRINT(2, "id:%04x,path:%s,func:%p,maxmsgsize:%lu,qlen:%lu,cpumask:",
	  id, path, func, maxmsgsize, qlen);
  if (verbose(2)) {
    cpusetDump(stderr, &cpumask);
  }

  fsrv = funcserver_new(id, path, func, maxmsgsize, qlen, cpumask);
  HASH_ADD_INT(Args.funcServers.hashtable, id, fsrv); 
  Args.funcCnt++;
  return true;
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
      if (!addFunc(argv, argc, optarg)) {
	fprintf(stderr, "%s", "ERROR: invalid function specification\n");
	usage(argv[0]);
	return false;
      }
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
		       Args.funcCnt);

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
  sockserver_t ssrv;
  funcserver_t fsrv, tmp;
  int i;
  
  if (!processArgs(argc,argv)) return -1;

  assert(Args.inputCnt == HASH_COUNT(Args.inputServers.hashtable));
  assert(Args.funcCnt == HASH_COUNT(Args.funcServers.hashtable));
  //  assert(Args.outputCnt == HASH_COUNT(Args.outputServers.hashtable));

  HASH_ITER(hh, Args.funcServers.hashtable, fsrv, tmp) {
    funcserver_start(fsrv,
		     true //asunc
		     );
  }

  for (i=1; i<Args.inputCnt; i++) {
    HASH_FIND_INT(Args.inputServers.hashtable, &i, ssrv);
    assert(ssrv!=NULL);
    sockserver_start(ssrv,
		     true // async
		     );
  }

  i=0;
  HASH_FIND_INT(Args.inputServers.hashtable, &i, ssrv);
  assert(ssrv!=NULL);
  // run the 0th socketserver on the main thread
  sockserver_start(ssrv,
		   false // async
		   );
  
  return 0;
}

