#define _GNU_SOURCE
#include "ssbench.h"


static void funcserver_setId(funcserver_t this, uint32_t id)
{
  this->id = id;
}

static void funcserver_setFunc(funcserver_t this, funcserver_func *func)
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

static void
funcserver_init(funcserver_t this, uint32_t id, funcserver_func *func,
	      size_t maxmsgsize, size_t qlen)
{
  funcserver_setId(this, id);
  funcserver_setFunc(this, func);
  funcserver_setMaxmsgsize(this, maxmsgsize);
  funcserver_setQlen(this, qlen);
  queue_init(&(this->queue),qlen);
}

funcserver_t
funcserver_new(uint32_t id, funcserver_func *func,
			size_t maxmsgsize, int qlen) {
  funcserver_t this;

  // for each queue entry required we need a queue_entry struct plus
  // enought bytes for the maximum  size of a single message for this
  // funceration.
  this = malloc(sizeof(struct funcserver) +
		((sizeof(struct queue_entry) + maxmsgsize) * qlen));
  assert(this);
  funcserver_init(this, id, func, maxmsgsize, qlen);
  return this;
}
