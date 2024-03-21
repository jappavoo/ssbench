#define _GNU_SOURCE
#include "ssbench.h"

//012345678901234567890123456789012345678901234567890123456789012345678901234567

#define WVLP(VL,fmt,...)				  \
  VLPRINT(VL,"%p:%d:%lx:" fmt, this, worker_getId(this),	\
	 worker_getTid(this), __VA_ARGS__)

static inline void
worker_setId(worker_t this, workerid_t id)
{
  this->id = id;
}

static inline void
worker_setOid(worker_t this, outputid_t oid)
{
  this->oid = oid;
}

static inline void
worker_setOwid(worker_t this, workerid_t owid)
{
  this->owid = owid;
}

static inline void
worker_setTid(worker_t this, pthread_t tid)
{
  this->tid = tid;
}

static inline void
worker_setPath(worker_t this, const char *path)
{
  this->path = path;
}

static inline void
worker_setFunc(worker_t this, ssbench_func_t func)
{
  this->func = func;
}

static inline void
worker_setMaxmsgsize(worker_t this, size_t maxmsgsize)
{
  this->maxmsgsize = maxmsgsize;
}

static inline void
worker_setQlen(worker_t this, size_t qlen)
{
  this->qlen = qlen;
}

static inline void
worker_setCpumask(worker_t this, cpu_set_t cpumask)
{
  this->cpumask = cpumask;
}

static inline void
worker_setSemid(worker_t this, semid_t semid)
{
  this->semid = semid;
}

static inline void
worker_setOutput(worker_t this, output_t output)
{
  this->output = output;
}

extern void
worker_dump(worker_t this, FILE *file)
{
  fprintf(file, "worker:%p id:%04hx tid:%ld maxmsgsize:%lu qlen:%lu "
	  "semid:%x oid=%04hx owid=%04hx output=%p func:%p(", this,
	  worker_getId(this),
	  worker_getTid(this),
	  worker_getMaxmsgsize(this),
	  worker_getQlen(this),
	  worker_getSemid(this),
	  worker_getOid(this),
	  worker_getOwid(this),
	  worker_getOutput(this),
	  worker_getFunc(this));
  
  const char *path = worker_getPath(this);
  if (path==NULL) {
    fprintf(file, "%p) cpumask:", path);
  } else {
    fprintf(file, "%s) cpumask:", path);
  }
  cpu_set_t mask=worker_getCpumask(this);
  cpusetDump(stderr, &mask);
  queue_dump(worker_getQueue(this),stderr);
}

extern QueueEntryFindRC_t
worker_getQueueEntry(worker_t this, union ssbench_msghdr *h, queue_entry_t *qe)
{
  queue_t q  = worker_getQueue(this);
  size_t len = h->fields.datalen;

  return queue_getEmptyEntry(q, true, &len, qe);
}

extern void
worker_putBackQueueEntry(worker_t this, queue_entry_t *qe)
{
  queue_t q = worker_getQueue(this);
  semid_t semid = worker_getSemid(this);
  queue_putBackFullEntry(q, *qe);
  *qe = NULL;
 retry:
  int rc = semop(semid, &semsignal, 1);
  if (rc<0) {
    if (errno == EINTR) goto retry;
    perror("semop:");
    EEXIT();
  }
}
    
static void
worker_init(worker_t this, workerid_t id, const char *path,
	    ssbench_func_t func, struct qdesc *qds, qid_t qdcount,
	    outputid_t oid, workerid_t owid, cpu_set_t cpumask)
{
  worker_setId(this, id);
  worker_setTid(this, -1);
  //  worker_setMaxmsgsize(this, maxmsgsize);
  //worker_setQlen(this, qlen);
  worker_setPath(this, path);
  worker_setFunc(this, func);
  worker_setCpumask(this, cpumask);
  worker_setSemid(this, -1);
  worker_setOid(this, oid);
  worker_setOwid(this, owid);
  worker_setNumq(this, qdcount);
  
  if (oid == ID_NULL) {
    worker_setOutput(this, NULL);
  } else {
    // lookup oid in hash
    output_t output;
    int intoid = oid;
    HASH_FIND_INT(Args.outputs.hashtable, &intoid, output);
    if (output == NULL) {
      EPRINT("ERROR: %s: Cannot find output:%04hx\n", __func__, oid);
      EEXIT();
    } else {
      worker_setOutput(this, output);
    }
    ASSERT(owid != ID_NULL);
  }
  //queue_init(&(this->queue), maxmsgsize, qlen);

  if (verbose(2)) worker_dump(this, stderr);
}

worker_t
worker_new(workerid_t id, const char *path, ssbench_func_t func,
	   struct qdesc *qds, qid_t qdcount, outputid_t oid, workerid_t owid,
	   cpu_set_t cpumask) {
  worker_t this;
  this = malloc(sizeof(struct worker) + (qdcount * sizeof(struct queue *)));
  ASSERT(this);
  worker_init(this, id, path, func, qds, qdcount, oid, owid, cpumask);
  return this;
}

static void *
worker_thread_loop(void * arg)
{
  worker_t this       = arg;
  workerid_t id       = worker_getId(this);
  pthread_t tid       = pthread_self();
  char *name          = worker_getName(this);
  unsigned int nsize  = worker_sizeofName(this);
  const char *path    = worker_getPath(this);
  queue_t iq          = worker_getQueue(this);
  ssbench_func_t func = worker_getFunc(this);
  output_t output     = worker_getOutput(this);
  outputid_t owid     = worker_getOwid(this);
  queue_entry_t iqe, oqe;
  union ssbench_msghdr *omh;
  size_t olen, flen;
  uint8_t *odata;
  int rc;
  
  worker_setTid(this, tid);
  snprintf(name,nsize,"fs%08x%.*s",id,nsize-10,path);
  pthread_setname_np(tid, name);

  int semid = semget(IPC_PRIVATE, 1, S_IRUSR|S_IWUSR);  
  if (semid < 0) {
    perror("semget:");
    ASSERT(0);
  }
  worker_setSemid(this, semid);
  
  /* initlize the semaphore (#0 in the set) count to 0 */
  rc = semctl(semid, 0, SETVAL, (int)0);
  ASSERT(rc==0);

  if (verbose(1)) {
    worker_dump(this,stderr);
    cpu_set_t cpumask;
    rc = pthread_getaffinity_np(tid, sizeof(cpumask), &cpumask);
    ASSERT(rc==0);
    WVLP(1,"%s", "cpuaffinity:");
    cpusetDump(stderr, &cpumask);
  }
  WVLP(1,"%s", "started\n");
  pthread_barrier_wait(&(Args.workers.barrier));

  for (;;) {
  retry:
    int rc = semop(semid, &semwait, 1);
    if (rc<0) {
      if (errno == EINTR) goto retry;
      perror("semop:");
      exit(-1);
    }
    WVLP(2, "%s", "AWAKE\n");
    // grab a full entry -- must exist if semaphore was signaled
    queue_getFullEntry(iq, &iqe);
    ASSERT(iqe);
    if (output != NULL) {
      if (verbose(2)) {
	output_dump(output,stderr);
      }
      QueueEntryFindRC_t qrc = output_getQueueEntry(output, &olen, &oqe);
      if (qrc == Q_FULL) {
	EPRINT("%s", "NO Output Queue entry available -- BACK PRESSURE??");
	NYI;
      } else {
	WVLP(2,"oqe=%p oqe->data=%p oqe->len:%lu olen=%lu\n",
	     oqe, oqe->data, oqe->len, olen);
	assert(olen > sizeof(union ssbench_msghdr));
	omh = (union ssbench_msghdr *)(oqe->data);
	omh->fields.wq.workerid = owid;
	omh->fields.datalen     = 0;
	odata = &(oqe->data[sizeof(union ssbench_msghdr)]);
	oqe->len = sizeof(union ssbench_msghdr);
	olen -= sizeof(union ssbench_msghdr);
      }
    } else {
      odata = NULL;
      olen = 0;
    }    
    // invoke the function on the input data and pass output buffer
    flen = func(iqe->data, iqe->len, odata, olen, this);
    // function done so we can put entry back on empty list
    if (output) {
      omh->fields.datalen = flen;
      oqe->len += flen;
      output_putBackQueueEntry(output, &oqe);
    }
    queue_putBackEmptyEntry(iq, iqe);
  }
  
}

void worker_start(worker_t this, bool async)
{
  pthread_t tid;
  int rc;
  cpu_set_t cpumask = worker_getCpumask(this);
  
  if (async) {
    pthread_attr_t attr;
    pthread_attr_t *attrp;
    
    attrp = &attr;
    rc = pthread_attr_init(attrp);
    ASSERT(rc==0);
    rc = pthread_attr_setaffinity_np(attrp, sizeof(cpumask), &cpumask);
    ASSERT(rc==0);
    // start thread loop
    rc = pthread_create(&tid, attrp, &worker_thread_loop, this);
    ASSERT(rc==0);
    rc = pthread_attr_destroy(attrp);
    ASSERT(rc==0);
  } else {
    // set and confirm affinity
    tid = pthread_self();
    rc = pthread_setaffinity_np(tid, sizeof(cpumask), &cpumask);
    ASSERT(rc==0);
    worker_thread_loop(this);
  }
}

extern void
worker_destroy(worker_t this)
{
  // FIXME: cleanup properly
  int semid = worker_getSemid(this);

  WVLP(1, "%s", "called\n");
  
  if (semid!=-1) {
    int rc = semctl(semid, 0, IPC_RMID);
    if (rc != 0) perror("IPC_RMID");
  }
}