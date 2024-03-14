#define _GNU_SOURCE
#include "ssbench.h"
#include <sys/ipc.h>
#include <sys/sem.h>
#include <fcntl.h>

//012345678901234567890123456789012345678901234567890123456789012345678901234567

#define FSVLP(VL,fmt,...)				  \
  VLPRINT(VL,"%p:%d:%lx:" fmt, this, funcserver_getId(this),	\
	 funcserver_getTid(this), __VA_ARGS__)

static inline void funcserver_setId(funcserver_t this, uint32_t id)
{
  this->id = id;
}

static inline void funcserver_setTid(funcserver_t this, pthread_t tid)
{
  this->tid = tid;
}

static inline void funcserver_setPath(funcserver_t this, const char *path)
{
  this->path = path;
}

static inline void funcserver_setFunc(funcserver_t this, ssbench_func_t func)
{
  this->func = func;
}

static inline void funcserver_setMaxmsgsize(funcserver_t this,
					    size_t maxmsgsize)
{
  this->maxmsgsize = maxmsgsize;
}

static inline void funcserver_setQlen(funcserver_t this, size_t qlen)
{
  this->qlen = qlen;
}

static inline void funcserver_setCpumask(funcserver_t this, cpu_set_t cpumask)
{
  this->cpumask = cpumask;
}

static inline void funcserver_setSemid(funcserver_t this, semid_t semid)
{
  this->semid = semid;
}

extern void
funcserver_dump(funcserver_t this, FILE *file)
{
  fprintf(file, "funcserver:%p id:%08x tid:%ld maxmsgsize:%lu qlen:%lu "
	  "semid:%x func:%p(", this,
	  funcserver_getId(this),
	  funcserver_getTid(this),
	  funcserver_getMaxmsgsize(this),
	  funcserver_getQlen(this),
	  funcserver_getSemid(this),
	  funcserver_getFunc(this));
  const char *path = funcserver_getPath(this);
  if (path==NULL) {
    fprintf(file, "%p) cpumask:", path);
  } else {
    fprintf(file, "%s) cpumask:", path);
  }
  cpu_set_t mask=funcserver_getCpumask(this);
  cpusetDump(stderr, &mask);
  queue_dump(funcserver_getQueue(this),stderr);
}

extern QueueEntryFindRC_t
funcserver_getQueueEntry(funcserver_t this,
			 union ssbench_msghdr *h,
			 queue_entry_t *qe)
{
  queue_t q  = funcserver_getQueue(this);
  size_t len = h->fields.datalen;

  return queue_getEmptyEntry(q, len, qe);
}

extern void
funcserver_putBackQueueEntry(funcserver_t this,
			     queue_entry_t *qe)
{
  queue_t q = funcserver_getQueue(this);
  int semid = funcserver_getSemid(this);
  queue_putBackFullEntry(q, *qe);
  *qe = NULL;
  int rc = semop(semid, &semsignal, 1);
  if (rc<0) {
    perror("semop:");
    exit(-1);
  }
}
    
static void
funcserver_init(funcserver_t this, uint32_t id, const char *path,
		ssbench_func_t func, size_t maxmsgsize, size_t qlen,
		cpu_set_t cpumask)
{
  funcserver_setId(this, id);
  funcserver_setTid(this, -1);
  funcserver_setMaxmsgsize(this, maxmsgsize);
  funcserver_setQlen(this, qlen);
  funcserver_setPath(this, path);
  funcserver_setFunc(this, func);
  funcserver_setCpumask(this, cpumask);
  funcserver_setSemid(this, -1);
  queue_init(&(this->queue), maxmsgsize, qlen);
}

funcserver_t
funcserver_new(uint32_t id, const char *path, ssbench_func_t func,
	       size_t maxmsgsize, int qlen, cpu_set_t cpumask) {
  funcserver_t this;

  // for each queue entry required we need a queue_entry struct plus
  // enought bytes for the maximum  size of a single message for this
  // funceration.
  this = malloc(sizeof(struct funcserver) +
		((sizeof(struct queue_entry) + maxmsgsize) * qlen));
  ASSERT(this);
  funcserver_init(this, id, path, func, maxmsgsize, qlen, cpumask);
  if (verbose(2)) funcserver_dump(this, stderr);
  return this;
}

static void * funcserver_thread_loop(void * arg)
{
  funcserver_t this = arg;
  int id = funcserver_getId(this);
  pthread_t tid = pthread_self();
  char *name = funcserver_getName(this);
  unsigned int nsize = funcserver_sizeofName(this);
  const char *path= funcserver_getPath(this);
  queue_t q  = funcserver_getQueue(this);
  ssbench_func_t func = funcserver_getFunc(this);
  queue_entry_t qe;
  int rc;
  
  funcserver_setTid(this, tid);
  snprintf(name,nsize,"fs%08x%.*s",id,nsize-10,path);
  pthread_setname_np(tid, name);

  int semid = semget(IPC_PRIVATE, 1, S_IRUSR|S_IWUSR);  
  if (semid < 0) {
    perror("semget:");
    ASSERT(0);
  }
  funcserver_setSemid(this, semid);
  
  /* initlize the semaphore (#0 in the set) count to 0 */
  rc = semctl(semid, 0, SETVAL, (int)0)
  ASSERT(rc==0);

  if (verbose(1)) {
    funcserver_dump(this,stderr);
    cpu_set_t cpumask;
    rc = pthread_getaffinity_np(tid, sizeof(cpumask), &cpumask);
    ASSERT(rc==0);
    FSVLP(1,"%s", "cpuaffinity:");
    cpusetDump(stderr, &cpumask);
  }
  FSVLP(1,"%s", "started\n");
  pthread_barrier_wait(&(Args.funcServers.barrier));

  for (;;) {
    int rc = semop(semid, &semwait, 1);
    if (rc<0) {
      perror("semop:");
      exit(-1);
    }
    FSVLP(2, "%s", "AWAKE\n");
    // grab a full entry -- must exist if semaphore was signaled
    queue_getFullEntry(q, &qe);
    ASSERT(qe);
    // invoke the function on the input data and pass output buffer NYI
    func(qe->data, qe->len, NULL, 0, this);
    // function done so we can put entry back on empty list
    queue_putBackEmptyEntry(q, qe);
  }
  
}

void funcserver_start(funcserver_t this, bool async)
{
  pthread_t tid;
  int rc;
  cpu_set_t cpumask = funcserver_getCpumask(this);
  
  if (async) {
    pthread_attr_t attr;
    pthread_attr_t *attrp;
    
    attrp = &attr;
    rc = pthread_attr_init(attrp);
    ASSERT(rc==0);
    rc = pthread_attr_setaffinity_np(attrp, sizeof(cpumask), &cpumask);
    ASSERT(rc==0);
    // start thread loop
    rc = pthread_create(&tid, attrp, &funcserver_thread_loop, this);
    ASSERT(rc==0);
    rc = pthread_attr_destroy(attrp);
    ASSERT(rc==0);
  } else {
    // set and confirm affinity
    tid = pthread_self();
    rc = pthread_setaffinity_np(tid, sizeof(cpumask), &cpumask);
    ASSERT(rc==0);
    funcserver_thread_loop(this);
  }
}

extern void
funcserver_destroy(funcserver_t this)
{
  // FIXME: cleanup properly
  int semid = funcserver_getSemid(this);

  FSVLP(1, "%s", "called\n");
  
  if (semid!=-1) {
    int rc = semctl(semid, 0, IPC_RMID);
    if (rc != 0) perror("IPC_RMID");
  }
}
