#define _GNU_SOURCE
#include "ssbench.h"


static void opserver_setId(opserver_t this, uint32_t id)
{
  this->id = id;
}

static void opserver_setFunc(opserver_t this, opserver_func *func)
{
  this->func = func;
}

static void opserver_setMaxmsgsize(opserver_t this, size_t maxmsgsize)
{
  this->maxmsgsize = maxmsgsize;
}

static void opserver_setQlen(opserver_t this, size_t qlen)
{
  this->qlen = qlen;
}

static void
opserver_init(opserver_t this, uint32_t id, opserver_func *func,
	      size_t maxmsgsize, size_t qlen)
{
  opserver_setId(this, id);
  opserver_setFunc(this, func);
  opserver_setMaxmsgsize(this, maxmsgsize);
  opserver_setQlen(this, qlen);
  queue_init(&(this->queue),qlen);
}

opserver_t
opserver_new(uint32_t id, opserver_func *func,
			size_t maxmsgsize, int qlen) {
  opserver_t this;

  // for each queue entry required we need a queue_entry struct plus
  // enought bytes for the maximum  size of a single message for this
  // operation.
  this = malloc(sizeof(struct opserver) +
		((sizeof(struct queue_entry) + maxmsgsize) * qlen));
  assert(this);
  opserver_init(this, id, func, maxmsgsize, qlen);
  return this;
}
