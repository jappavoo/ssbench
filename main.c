// _GNU_SOURCE is required to pickup getcpu
#define _GNU_SOURCE
#include "ssbench.h"
#include <errno.h>
#include <getopt.h>
#include <sys/sysinfo.h>
#include <signal.h>

//012345678901234567890123456789012345678901234567890123456789012345678901234567
static void usage(char *name)
{
  fprintf(stderr,
	  "USAGE: %s [-v] [-i port[,cpu[,cpu,...]] "
	  "-o id,addr:port,maxmsgsize,qlen[,cpu[,cpu...]]\n"		
	  "[-f id,func,maxmsgsize,qlen,outputid,outputfunctionid"
	  "[,cpu[,cpu...]]\n "
	  "  demux messages from one or more input ports to a specified "
	  "function and can establishes output connection for the functions"
	  " to send messages to.\n",
	  name);
}

extern queue_scanstate_t rr_queue_scanstate_init(void);
extern queue_scanstate_t rr_queue_scanfull(queue_t qs[], qid_t n,
					   qid_t *idx, queue_entry_t *qe,
					   queue_scanstate_t ss);

//extern queue_scanstate_init_func_t rr_queue_scanstate_init;
//extern queue_scanfull_func_t rr_queue_scanfull;

struct Args Args = {
  .inputCnt             = 0,
  .outputCnt            = 0,
  .workerCnt            = 0,
  .verbose              = 0,
  .totalcpus            = 0,
  .availcpus            = 0,
  .pid                  = 0,
  .queue_scanstate_init = NULL,
  .queue_scanfull       = NULL,
  .inputs.hashtable     = NULL,
  .outputs.hashtable    = NULL,
  .workers.hashtable    = NULL
};

static void
dumpOutputs()
{
  output_t o, tmp;
  
  fprintf(stderr, "Args.outputs.hashtable:\n");
  HASH_ITER(hh, Args.outputs.hashtable, o, tmp) {
    output_dump(o, stderr);
  }
}

static void
dumpInputs()
{
  input_t i, tmp;

  fprintf(stderr, "Args.inputs.hashtable:\n");
  HASH_ITER(hh, Args.inputs.hashtable, i, tmp) {
    input_dump(i, stderr);
  }
}


static void
dumpWorkers()
{
  worker_t  w, tmp;
  
  fprintf(stderr, "Args.workers.hashtable:\n");
  HASH_ITER(hh, Args.workers.hashtable, w, tmp) {
    worker_dump(w, stderr);
  }
}

static void
dumpArgs()
{
  fprintf(stderr, "Args.pid=%d Args.totalcpus=%u, Args.availcpus=%u\n"
	  "Args.inputCnt=%d Args.outputCnt=%d Args.workerCnt=%d\n"
	  "Args.inputs.ht=%p, Args.outputs.ht=%p Args.workers.ht=%p\n"
	  "Args.queue_scanstate_init=%p, Args.queue_scanfull=%p\n"
	  "Args.verbose=%d\n",
	  Args.pid, Args.totalcpus, Args.availcpus,
	  Args.inputCnt,
	  Args.outputCnt,
	  Args.workerCnt,
	  Args.inputs.hashtable,
	  Args.outputs.hashtable,
	  Args.workers.hashtable,
	  Args.queue_scanstate_init, Args.queue_scanfull,
	  Args.verbose);
  dumpInputs();
  dumpOutputs();
  dumpWorkers();
}

static bool
parseCPUSet(char *str, cpu_set_t * mask)
{
  int i=0;
  char *endptr;
  
  if (str) {
    while (*str != '\0') {
      errno = 0;
      int cpu = strtol(str, &endptr, 0);
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
	  EPRINT("%d greater than totalcpus on the system: %d\n",
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

static bool
addInput(char **argv, int argc, char *optarg)
{
  int port;
  char *endptr;
  cpu_set_t cpumask;	
  inputid_t id = Args.inputCnt;
  input_t input;
  
  errno = 0;   // as per man page notes for strtod or strtol
  port = strtol(optarg, &endptr, 0);
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
  int intid = id;
  HASH_FIND_INT(Args.inputs.hashtable, &intid, input);
  assert(input==NULL);
  input = input_new(port, id, cpumask);
  HASH_ADD_INT(Args.inputs.hashtable, id, input);   
  Args.inputCnt++;
  return true;
}

static bool
addOutput(char **argv, int argc, char *optarg)
{
  char *sptr;
  char *endptr;
  char *tmpstr;
  output_t output;
  int rc;
  
  outputid_t id;
  tmpstr = strtok_r(optarg, ",", &sptr);
  if (tmpstr == NULL) return false;
  errno = 0;   // as per man page notes for strtod or strtol
  id = strtol(tmpstr, &endptr, 0);
  if (endptr == tmpstr || errno != 0) return false;
  /* id already in the hash? */
  int intid = id;
  HASH_FIND_INT(Args.outputs.hashtable, &intid, output); 
  if (output != NULL) {
    EPRINT("ERROR: %04hx already defined as a output id\n", id);
    output_dump(output,stderr);
    return false;
  }

  char *host = strtok_r(NULL, ":", &sptr);
  if (host == NULL) return false;
  int port;
  tmpstr = strtok_r(NULL, ",", &sptr);
  if (tmpstr == NULL) return false;
  errno = 0;   // as per man page notes for strtod or strtol
  port = strtol(tmpstr, &endptr, 0);
  if (endptr == tmpstr || errno != 0) return false;
  int ofd;
  rc = net_setup_connection(&ofd, host, port);
  if (rc != 1 ) {
    EPRINT("ERROR:failed to create output connection to %s:%d\n", host, port);
    exit(-1);
  }
  
  size_t maxmsgsize;
  tmpstr = strtok_r(NULL, ",", &sptr);
  if (tmpstr == NULL) return false;
  errno = 0;   // as per man page notes for strtod or strtol
  maxmsgsize = strtol(tmpstr, &endptr, 0);
  if (endptr == tmpstr || errno != 0) return false;
  
  size_t qlen;
  tmpstr = strtok_r(NULL, ",", &sptr);
  if (tmpstr == NULL) return false;
  errno = 0;   // as per man page notes for strtod or strtol
  qlen = strtol(tmpstr, &endptr, 0);
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
  
  VLPRINT(2, "id:%04x,host:%s,port:%d,ofd:%d,maxmsgsize:%lu,qlen:%lu,cpumask:",
	  id, host, port, ofd,  maxmsgsize, qlen);
  if (verbose(2)) {
    cpusetDump(stderr, &cpumask);
  }

  output = output_new(id, host, port, ofd, maxmsgsize, qlen, cpumask);
  HASH_ADD_INT(Args.outputs.hashtable, id, output); 
  Args.outputCnt++;
  return true;
}

static bool
parseQSpec(char *str, queue_desc_t *qds, qid_t *n)
{
  char         *sptr, *ptr, *tmpstr, *endptr;
  size_t        maxentrysize;
  size_t        qlen;
  qid_t         count;
  qid_t         num = 0; 
  queue_desc_t  qd  = NULL;;
  ptr = str;

  do {
    tmpstr = strtok_r(ptr, ":", &sptr);
    if (tmpstr == NULL) return false;
    errno = 0;
    count  = strtol(tmpstr, &endptr, 0);
    if (endptr == tmpstr || errno != 0) break;
    
    tmpstr = strtok_r(NULL, ":", &sptr);
    errno = 0;   // as per man page notes for strtod or strtol
    maxentrysize = strtol(tmpstr, &endptr, 0);
    if (endptr == tmpstr || errno != 0) break;
    
    tmpstr = strtok_r(NULL, ",", &sptr);
    if (tmpstr == NULL)  break;
    errno = 0;   // as per man page notes for strtod or strtol
    qlen = strtol(tmpstr, &endptr, 0);
    if (endptr == tmpstr || errno != 0) break;
    VLPRINT(2, "%d:count:%hd maxentrysize:%lu qlen:%lu\n",
	    num, count, maxentrysize, qlen);
    ptr = NULL;
    if (num==0) qd = malloc(sizeof(struct queue_desc));
    else qd = reallocarray(qd, num+1, sizeof(struct queue_desc));
    qd[num].count        = count;
    qd[num].maxentrysize = maxentrysize;
    qd[num].qlen         = qlen;
    num++;  
  } while (*sptr != '\0');
  if (*sptr != 0) {
    // error must have happened
    free(qd);
    return false;
  }
  *qds = qd;
  *n   = num;  
  return true;    
}

static bool
addWorker(char **argv, int argc, char *optarg)
{
  char *sptr;
  char *endptr;
  char *tmpstr;
  worker_t w;
  
  workerid_t id;
  tmpstr = strtok_r(optarg, ",", &sptr);
  if (tmpstr == NULL) return false;
  errno = 0;   // as per man page notes for strtod or strtol
  id = strtol(tmpstr, &endptr, 0);
  if (endptr == tmpstr || errno != 0) return false;
  /* id already in the hash? */
  int intid = id;
  HASH_FIND_INT(Args.workers.hashtable, &intid, w); 
  if (w != NULL) {
    EPRINT("ERROR: %04x already defined as a worker id\n", id);
    worker_dump(w,stderr);
    return false;
  }

  ssbench_func_t func = NULL;
  char *path = strtok_r(NULL, ",", &sptr);
  if (path == NULL) return false;
  func = func_getfunc(path);
  if (func == NULL) {
    EPRINT("WARNING: no function associated with %s. Will use func=%p\n",
	   path, func);
  }

  queue_desc_t qds;
  qid_t qdcount;
  tmpstr = strtok_r(NULL, "[]", &sptr);
  if (tmpstr == NULL) {
    EPRINT("%s", "ERROR: queue specification must be within square brackets\n");
    return false;
  }
  if (parseQSpec(tmpstr, &qds, &qdcount) == false) return false;

  outputid_t oid;
  workerid_t owid;
  qid_t      oqid;
  cpu_set_t cpumask;
  CPU_ZERO(&cpumask);
  tmpstr = strtok_r(NULL, ",", &sptr);
  if (tmpstr != NULL) {
    errno = 0;   // as per man page notes for strtod or strtol
    oid = strtol(tmpstr, &endptr, 0);
    if (endptr == tmpstr || errno != 0) oid = ID_NULL;

    tmpstr = strtok_r(NULL, ",", &sptr);
    if (tmpstr == NULL) return false;
    errno = 0;   // as per man page notes for strtod or strtol
    owid = strtol(tmpstr, &endptr, 0);
    if (endptr == tmpstr || errno != 0) owid = ID_NULL;

    tmpstr = strtok_r(NULL, ",", &sptr);
    if (tmpstr == NULL) return false;
    errno = 0;   // as per man page notes for strtod or strtol
    oqid = strtol(tmpstr, &endptr, 0);
    if (endptr == tmpstr || errno != 0) oqid = ID_NULL;
    if (!QID_ISVALID(oqid)) {
      EPRINT("ERROR: bad output qid %hd\n", oqid);
      return false;
    }
    
    if (*sptr != '\0') {
      if (!parseCPUSet(sptr, &cpumask)) {
	fprintf(stderr, "ERROR: invalid cpu set specifiation: %s\n", sptr);
	usage(argv[0]);
	return false;
      }
    } else {
      setAllcpus(Args.totalcpus, &cpumask);
    }
  } else {
    // if no cpu mask specified then set mask to all cpus
    setAllcpus(Args.totalcpus, &cpumask);
    oid  = ID_NULL;
    oqid = ID_NULL;
    owid = ID_NULL; 
  }
  VLPRINT(2, "id:%04x,path:%s,func:%p,oid:%04x,owid:%04x,queues:",
	  id, path, func, oid, owid);
  if (verbose(2)) {
    queue_desc_dump(stderr, qds, qdcount);
    VLPRINT(2, "%s",",cpumask:")
    cpusetDump(stderr, &cpumask);
    VLPRINT(2, "%s", "\n");
  }
  w = worker_new(id, path, func, qds, qdcount,oid, owid, oqid, cpumask);
  HASH_ADD_INT(Args.workers.hashtable, id, w); 
  Args.workerCnt++;
  return true;
}

static bool
processArgs(int argc, char **argv)
{
  char opt;

  unsigned int  cpu, node;

  Args.pid                  = getpid();
  Args.queue_scanstate_init = rr_queue_scanstate_init;
  Args.queue_scanfull       = rr_queue_scanfull;
  Args.totalcpus            = get_nprocs_conf();
  Args.availcpus            = get_nprocs();
  
  getcpu(&cpu, &node);

  while ((opt = getopt(argc, argv, "hi:w:o:v")) != -1) {
    switch (opt) {
    case 'w':
      if (!addWorker(argv, argc, optarg)) {
	fprintf(stderr, "%s", "ERROR: invalid worker specification\n");
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
    EPRINT("ERROR:%d:Require at least one -i <inputport>[,cpu...]"
	   "input port server thread\n", Args.inputCnt);
    usage(argv[0]);
    return false;
  }

  pthread_barrier_init(&Args.workers.barrier, NULL,
		       Args.workerCnt);

  pthread_barrier_init(&Args.inputs.barrier, NULL,
		       Args.inputCnt);

  pthread_barrier_init(&Args.outputs.barrier, NULL,
		       Args.outputCnt);

#if 0
  // example if we want required command line arguments beyond the
  // coammand line switches processed above
  if ((argc - optind) < 3) {
    usage(argv[0]);
    return false;
  }

  Args.something1 = argv[optind];
  Args.something2 = argv[optind+1];
  Args.something3 = argv[optind+2];
#endif
  
  return true;
}

extern 
void cleanup(void)
{
  input_t  input,  itmp;
  output_t output, otmp;
  worker_t worker, wtmp;

  HASH_ITER(hh, Args.inputs.hashtable, input, itmp) {
    input_destroy(input);
    HASH_DEL(Args.inputs.hashtable, input); 
    free(input);         
  }

  HASH_ITER(hh, Args.outputs.hashtable, output, otmp) {
    output_destroy(output);
    HASH_DEL(Args.outputs.hashtable, output); 
    free(output);         
  }

  HASH_ITER(hh, Args.workers.hashtable, worker, wtmp) {
    worker_destroy(worker);
    HASH_DEL(Args.workers.hashtable, worker); 
    free(worker);         
  }
}

void signalhandler(int num)
{
  VLPRINT(1, "num:%d\n", num);
  cleanup();
}

int main(int argc, char **argv)
{
  input_t   input;
  output_t  output, otmp;
  worker_t  worker, wtmp;
  int i;
  
  if (!processArgs(argc,argv)) return -1;

  assert(Args.inputCnt == HASH_COUNT(Args.inputs.hashtable));
  assert(Args.workerCnt == HASH_COUNT(Args.workers.hashtable));
  assert(Args.outputCnt == HASH_COUNT(Args.outputs.hashtable));

  atexit(cleanup);
  signal(SIGTERM, signalhandler);
  signal(SIGINT, signalhandler);
  
  HASH_ITER(hh, Args.workers.hashtable, worker, wtmp) {
    worker_start(worker,
		 true //asunc
		 );
  }

  HASH_ITER(hh, Args.outputs.hashtable, output, otmp) {
    output_start(output,
		 true //asunc
		 );
  }
  
  for (i=1; i<Args.inputCnt; i++) {
    HASH_FIND_INT(Args.inputs.hashtable, &i, input);
    assert(input != NULL);
    input_start(input,
		true // async
		);
  }

  i=0;
  HASH_FIND_INT(Args.inputs.hashtable, &i, input);
  assert(input != NULL);
  // run the 0th socketserver on the main thread
  input_start(input,
	      false // async
	      );
  
  return 0;
}

