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
#define OSVLP(VL,fmt,...)				  \
  VLPRINT(VL,"%p:%d:%lx:" fmt, this, outputserver_getId(this),	\
	 outputserver_getTid(this), __VA_ARGS__)

static inline void outputserver_setPort(outputserver_t this, int p)
{
  this->port = p;
}

static inline void outputserver_setSendfd(outputserver_t this, int fd)
{
  this->sendFd = fd;
}

static inline void outputserver_setId(outputserver_t this, int id)
{
  this->id = id;
}

static inline void outputserver_setTid(outputserver_t this, pthread_t tid)
{
  this->tid = tid;
}

static inline void outputserver_setMsgcnt(outputserver_t this, int c)
{
  this->msgcnt = c;
}

static inline void outputserver_incMsgcnt(outputserver_t this, int c)
{
  this->msgcnt++;
}

static inline void outputserver_setCpumask(outputserver_t this,
					   cpu_set_t cpumask) {
  this->cpumask = cpumask;
}

static inline void outputserver_setHost(outputserver_t this, const char *host)
{
  this->host = host;
}

static inline void outputserver_setMaxmsgsize(outputserver_t this,
					     size_t maxmsgsize)
{
  this->maxmsgsize = maxmsgsize;
}

static inline void outputserver_setQlen(outputserver_t this, size_t qlen)
{
  this->qlen = qlen;
}

static inline void outputserver_setSemid(outputserver_t this, int semid)
{
  this->semid = semid;
}

void
outputserver_dump(outputserver_t this, FILE *file)
{
  fprintf(file, "outputserver:%p id:%d tid:%ld host:%s port:%d SendFd:%d "
	  "MaxMsgSize:%ld qlen:%ld\n", this,
	  outputserver_getId(this),
	  outputserver_getTid(this),
	  outputserver_getHost(this),
	  outputserver_getPort(this),
	  outputserver_getSendfd(this),
	  outputserver_getMaxmsgsize(this),
	  outputserver_getQlen(this));
}

#define MAX_EVENTS 1024
// epoll code is based on example from the man page
static void * outputserver_thread_loop(void * arg)
{
  outputserver_t this = arg;
  int id = outputserver_getId(this);
  pthread_t tid = pthread_self();
  char *name = outputserver_getName(this);
  unsigned int nsize = outputserver_sizeofName(this);
  queue_t q  = outputserver_getQueue(this);
  PortType port = outputserver_getPort(this);
  int sendfd = outputserver_getSendfd(this);
  queue_entry_t qe;

  outputserver_setTid(this, tid);
  snprintf(name,nsize,"os%08x%06d",id,port);
  pthread_setname_np(tid, name);

  int semid = semget(IPC_PRIVATE, 1, S_IRUSR|S_IWUSR);  
  if (semid < 0) {
    perror("semget:");
    assert(0);
  }
  outputserver_setSemid(this, semid);

  /* initlize the semaphore (#0 in the set) count to 0 Simulate a mutex */
  assert(semctl(semid, 0, SETVAL, (int)0)==0);

  if (verbose(1)) {
    outputserver_dump(this,stderr);
    cpu_set_t cpumask;
    assert(pthread_getaffinity_np(tid, sizeof(cpumask), &cpumask)==0);
    OSVLP(1,"%s", "cpuaffinity:");
    cpusetDump(stderr, &cpumask);
  }
  OSVLP(1,"%s", "started\n");
  pthread_barrier_wait(&(Args.outputServers.barrier));
  
  for (;;) {
    int rc = semop(semid, &semwait, 1);
    if (rc<0) {
      perror("semop:");
      exit(-1);
    }
    OSVLP(2, "%s", "AWAKE\n");
    queue_getFullEntry(q, &qe);
    assert(qe);
    net_writen(sendfd, qe->data, qe->len); 
    queue_putBackEmptyEntry(q, qe);
  }
}

void outputserver_start(outputserver_t this, bool async)
{
  pthread_t tid;
  int rc;
  cpu_set_t cpumask = outputserver_getCpumask(this);
  
  if (async) {
    pthread_attr_t attr;
    pthread_attr_t *attrp;
    
    attrp = &attr;
    rc = pthread_attr_init(attrp);
    assert(rc==0);
    rc = pthread_attr_setaffinity_np(attrp, sizeof(cpumask), &cpumask);
    assert(rc==0);
    // start thread loop
    rc = pthread_create(&tid, attrp, &outputserver_thread_loop, this);
    assert(rc==0);
    rc = pthread_attr_destroy(attrp);
    assert(rc==0);
  } else {
    // set and confirm affinity
    tid = pthread_self();
    rc = pthread_setaffinity_np(tid, sizeof(cpumask), &cpumask);
    assert(rc==0);
    outputserver_thread_loop(this);
  }
}

extern void
outputserver_destroy(outputserver_t this)
{
  OSVLP(1,"%s", "called\n");
}

static void
outputserver_init(outputserver_t this, int id, const char *host,
		  int port, int fd, size_t maxmsgsize, size_t qlen,
		  cpu_set_t cpumask, size_t qentrysizemax)
{
  outputserver_setSendfd(this, fd);
  outputserver_setHost(this, host);
  outputserver_setPort(this, port);
  outputserver_setId(this, id);
  outputserver_setMsgcnt(this, 0);
  outputserver_setMaxmsgsize(this, 0);
  outputserver_setQlen(this, qlen);
  outputserver_setCpumask(this, cpumask);
  queue_init(&(this->queue), qentrysizemax, qlen);
  
  if (verbose(2)) outputserver_dump(this, stderr);
}

outputserver_t
outputserver_new(int id, const char *host, int port, int fd, size_t maxmsgsize,
		 size_t qlen, cpu_set_t cpumask) {
  outputserver_t this;
  // the data of output bound queue entries need to contain both
  // a message header and the message data
  const unsigned int maxqentrysize = sizeof(union ssbench_msghdr) + maxmsgsize; 
  const unsigned int qbytes = ((sizeof(struct queue_entry) + maxqentrysize)				
			       * qlen);
  this = malloc(sizeof(struct outputserver) + qbytes);
  assert(this);
  outputserver_init(this, id, host, port, fd, maxmsgsize, qlen, cpumask,
		    maxqentrysize);
  return this;
}


