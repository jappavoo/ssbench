#define _GNU_SOURCE
#include "ssbench.h"
#include "hexdump.h"
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <fcntl.h>

//012345678901234567890123456789012345678901234567890123456789012345678901234567
#define OVLP(VL,fmt,...)				  \
  VLPRINT(VL,"%p:%d:%lx:" fmt, this, output_getId(this),	\
	 output_getTid(this), __VA_ARGS__)

static inline void output_setPort(output_t this, int p)
{
  this->port = p;
}

static inline void output_setSendfd(output_t this, int fd)
{
  this->sendFd = fd;
}

static inline void output_setId(output_t this, outputid_t id)
{
  this->id = id;
}

static inline void output_setTid(output_t this, pthread_t tid)
{
  this->tid = tid;
}

static inline void output_setMsgcnt(output_t this, int c)
{
  this->msgcnt = c;
}

static inline void output_incMsgcnt(output_t this, int c)
{
  this->msgcnt++;
}

static inline void output_setCpumask(output_t this, cpu_set_t cpumask)
{
  this->cpumask = cpumask;
}

static inline void output_setHost(output_t this, const char *host)
{
  this->host = host;
}

static inline void output_setMaxmsgsize(output_t this, size_t maxmsgsize)
{
  this->maxmsgsize = maxmsgsize;
}

static inline void output_setQlen(output_t this, size_t qlen)
{
  this->qlen = qlen;
}

static inline void output_setSemid(output_t this, int semid)
{
  this->semid = semid;
}

void
output_dump(output_t this, FILE *file)
{
  fprintf(file, "output:%p id:%d tid:%ld host:%s port:%d SendFd:%d "
	  "MaxMsgSize:%ld qlen:%ld\n", this,
	  output_getId(this),
	  output_getTid(this),
	  output_getHost(this),
	  output_getPort(this),
	  output_getSendfd(this),
	  output_getMaxmsgsize(this),
	  output_getQlen(this));
  queue_dump(output_getQueue(this),stderr);
}

#define MAX_EVENTS 1024
// epoll code is based on example from the man page
static void * output_thread_loop(void * arg)
{
  output_t this = arg;
  int id = output_getId(this);
  pthread_t tid = pthread_self();
  char *name = output_getName(this);
  unsigned int nsize = output_sizeofName(this);
  queue_t q  = output_getQueue(this);
  PortType port = output_getPort(this);
  int sendfd = output_getSendfd(this);
  queue_entry_t qe;

  output_setTid(this, tid);
  snprintf(name,nsize,"os%08x%06d",id,port);
  pthread_setname_np(tid, name);

  int semid = semget(IPC_PRIVATE, 1, S_IRUSR|S_IWUSR);  
  if (semid < 0) {
    perror("semget:");
    assert(0);
  }
  output_setSemid(this, semid);

  /* initlize the semaphore (#0 in the set) count to 0 Simulate a mutex */
  assert(semctl(semid, 0, SETVAL, (int)0)==0);

  if (verbose(1)) {
    output_dump(this,stderr);
    cpu_set_t cpumask;
    assert(pthread_getaffinity_np(tid, sizeof(cpumask), &cpumask)==0);
    OVLP(1,"%s", "cpuaffinity:");
    cpusetDump(stderr, &cpumask);
  }
  OVLP(1,"%s", "started\n");
  pthread_barrier_wait(&(Args.outputs.barrier));
  
  for (;;) {
  retry:
    int rc = semop(semid, &semwait, 1);
    if (rc<0) {
      if (errno == EINTR) goto retry;
      perror("semop:");
      exit(-1);
    }
    OVLP(2, "%s", "AWAKE\n");
    queue_getFullEntry(q, &qe);
    assert(qe);
    net_writen(sendfd, qe->data, qe->len); 
    queue_putBackEmptyEntry(q, qe);
  }
}

void output_start(output_t this, bool async)
{
  pthread_t tid;
  int rc;
  cpu_set_t cpumask = output_getCpumask(this);
  
  if (async) {
    pthread_attr_t attr;
    pthread_attr_t *attrp;
    
    attrp = &attr;
    rc = pthread_attr_init(attrp);
    assert(rc==0);
    rc = pthread_attr_setaffinity_np(attrp, sizeof(cpumask), &cpumask);
    assert(rc==0);
    // start thread loop
    rc = pthread_create(&tid, attrp, &output_thread_loop, this);
    assert(rc==0);
    rc = pthread_attr_destroy(attrp);
    assert(rc==0);
  } else {
    // set and confirm affinity
    tid = pthread_self();
    rc = pthread_setaffinity_np(tid, sizeof(cpumask), &cpumask);
    assert(rc==0);
    output_thread_loop(this);
  }
}

extern void
output_destroy(output_t this)
{
  int semid = output_getSemid(this);
  OVLP(1,"%s", "called\n");

  if (semid!=-1) {
    int rc = semctl(semid, 0, IPC_RMID);
    if (rc != 0) perror("IPC_RMID");
  }
}

static void
output_init(output_t this, outputid_t id, const char *host,
	    int port, int fd, size_t maxmsgsize, size_t qlen,
	    cpu_set_t cpumask, size_t qentrysizemax)
{
  output_setSendfd(this, fd);
  output_setHost(this, host);
  output_setPort(this, port);
  output_setId(this, id);
  output_setMsgcnt(this, 0);
  output_setMaxmsgsize(this, 0);
  output_setQlen(this, qlen);
  output_setCpumask(this, cpumask);
  queue_init(&(this->queue), qentrysizemax, qlen);
  
  if (verbose(2)) output_dump(this, stderr);
}

output_t
output_new(outputid_t id, const char *host, int port, int fd, size_t maxmsgsize,
	   size_t qlen, cpu_set_t cpumask) {
  output_t this;
  // the data of output bound queue entries need to contain both
  // a message header and the message data
  const unsigned int maxqentrysize = sizeof(union ssbench_msghdr) + maxmsgsize; 
  const unsigned int qbytes = ((sizeof(struct queue_entry) + maxqentrysize)				
			       * qlen);
  this = malloc(sizeof(struct output) + qbytes);
  assert(this);
  output_init(this, id, host, port, fd, maxmsgsize, qlen, cpumask,
	      maxqentrysize);
  return this;
}


