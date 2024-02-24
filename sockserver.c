#include "ssbench.h"

void
sockserver_init(sockserver_t this, int port)
{
  sockserver_setPort(this, port);
}

sockserver_t
sockserver_new(int port) {
  sockserver_t this;
  this = malloc(sizeof(struct sockserver));
  assert(this);
  sockserver_init(this, port);
  return this;
}

void
sockserver_dump(sockserver_t this, FILE *file)
{
  fprintf(file, "this:%p port:%d\n", this, sockserver_getPort(this));
}
