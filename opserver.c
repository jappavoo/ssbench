#include "ssbench.h"

opserver_t ophashtable = NULL;

static void opsever_setId(opserver_t this, uint32_t id)
{
  this->id = id;
}
static void opserver_init(opserver_t this, uint32_t id, opserver_func *func,
			  size_t maxmsgsize, size_t qlen)
{
  opserver_setId(this, id);
  opserver_setFunc(this, func);
  opserver_setMaxmsgsize(this, maxmsgsize);
  opserver_setQlen(this, qlen);
  queue_init(&(this->queue),qlen);
  HASH_ADD(hh, ophashtable, id, sizeof(this->id), this);
}

opserver_t opserver_new(uint32_t id, opserver_func *func,
			size_t maxmsgsize, size_t qlen) {
  opserver_t this;

  // for each queue entry required we need a queue_entry struct plus
  // enought bytes for the maximum  size of a single message for this
  // operation.
  this = malloc(sizeof(struct opserver) +
		((sizeof(struct queue_entry) + maxmsgsize) * qlen));
  assert(this);
  opserver_init(this, id, func, maxmsgsize, qlen);
  
}
