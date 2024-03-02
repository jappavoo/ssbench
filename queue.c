#define _GNU_SOURCE
#include "ssbench.h"

void queue_init(queue_t this, int qlen)
{
  this->full = NULL;
  
}

void queue_dump(queue_t this, FILE *file)
{
}
