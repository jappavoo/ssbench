#define _GNU_SOURCE
#include "ssbench.h"
//012345678901234567890123456789012345678901234567890123456789012345678901234567

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
  cpusetDump(stderr, funcserver_getCpumask(this));
  queue_dump(funcserver_getQueue(this),stderr);
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
  funcserver_setSemid(this, 0);
  queue_init(&(this->queue),qlen);
  
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
  assert(this);
  funcserver_init(this, id, path, func, maxmsgsize, qlen, cpumask);
  if (verbose(2)) funcserver_dump(this, stderr);
  return this;
}
