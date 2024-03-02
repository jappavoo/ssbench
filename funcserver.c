#define _GNU_SOURCE
#include "ssbench.h"


static void funcserver_setId(funcserver_t this, uint32_t id)
{
  this->id = id;
}

static void funcserver_setFunc(funcserver_t this, ssbench_func_t func)
{
  this->func = func;
}

static void funcserver_setMaxmsgsize(funcserver_t this, size_t maxmsgsize)
{
  this->maxmsgsize = maxmsgsize;
}

static void funcserver_setQlen(funcserver_t this, size_t qlen)
{
  this->qlen = qlen;
}

static void funcserver_setCpumask(funcserver_t this, cpu_set_t cpumask)
{
  this->cpumask = cpumask;
}

static void funcserver_setSemid(funcserver_t this, semid_t semid)
{
  this->semid = semid;
}

void funcserver_dump(funcserver_t this, FILE *file)
{
  fprintf(file, "funcserver:%p id:%08x tid:%ld maxmsgsize:%lu qlen:%lu "
	  //"path:%s func:%p semid:%x cpumask:"
	  , this,
	  funcserver_getId(this), funcserver_getTid(this),
	  funcserver_getMaxmsgsize(this), funcserver_getQlen(this)
	  //,
	  //	  funcserver_getPath(this), funcserver_getFunc(this),
	  //	  funcserver_getSemid(this));
	  );
  cpusetDump(stderr, funcserver_getCpumask(this));
  queue_dump(funcserver_getQueue(this),stderr);
}

static void
funcserver_init(funcserver_t this, uint32_t id, ssbench_func_t func,
		size_t maxmsgsize, size_t qlen, cpu_set_t cpumask)
{
  funcserver_setId(this, id);
  funcserver_setFunc(this, func);
  funcserver_setMaxmsgsize(this, maxmsgsize);
  funcserver_setQlen(this, qlen);
  funcserver_setCpumask(this, cpumask);
  funcserver_setSemid(this, 0);
  queue_init(&(this->queue),qlen);
}

funcserver_t
funcserver_new(uint32_t id, ssbench_func_t func,size_t maxmsgsize,
	       int qlen, cpu_set_t cpumask) {
  funcserver_t this;

  // for each queue entry required we need a queue_entry struct plus
  // enought bytes for the maximum  size of a single message for this
  // funceration.
  this = malloc(sizeof(struct funcserver) +
		((sizeof(struct queue_entry) + maxmsgsize) * qlen));
  assert(this);
  funcserver_init(this, id, func, maxmsgsize, qlen, cpumask);
  if (verbose(1)) funcserver_dump(this, stderr);
  return this;
}
