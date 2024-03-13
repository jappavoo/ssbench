#define _GNU_SOURCE
#include "ssbench.h"
#include "hexdump.h"

static inline 
queue_entry_t queue_next(queue_t this, queue_entry_t qe)
{
  uint8_t *ptr = (uint8_t *)qe;
  ptr += queue_entrysize(this);
  return (queue_entry_t) ptr;
}

extern void queue_init(queue_t this, size_t maxentrysize, size_t qlen)
{
  this->maxentrysize = maxentrysize;
  assert(qlen > 0);
  this->qlen = qlen;

  // initialize free list  
  queue_entry_t tmp = queue_getentryi(this,0);
  tmp->len    = 0;
  this->empty = tmp;

  if (qlen >= 2) {
    // only first and last node
    tmp->next = queue_getentryi(this,1);
    if (qlen > 2) {
      // iterate over the "middle" entries
      // tmp is point to first entry    
      for (int i=1;i<qlen-1; i++) {
	tmp = queue_getentryi(this,i);
	tmp->len  = 0;
	tmp->next = queue_getentryi(this,i+1);
      }
    }
  }
  // take care of last node (which might be first if only one element) 
  tmp = &(this->entries[qlen-1]);
  tmp->len  = 0;
  tmp->next = NULL;

  // initialize busy list
  this->full = NULL;
  if (verbose(2)) {
    queue_dump(this, stderr);
  }
}

void queue_entry_dump(queue_entry_t qe, FILE *file)
{
  fprintf(file, "  qe:%p len:%lu next:%p data:", qe, qe->len, qe->next);
  hexdump(file, qe->data, qe->len);
  if (qe->len==0) fprintf(stderr, "\n");
}

void queue_dump(queue_t this, FILE *file)
{
  size_t qesize = sizeof(struct queue_entry) + this->maxentrysize;
  fprintf(file, "queue:%p maxentrysize:%lu qlen:%lu sizeof(qe+data)=%lu(0x%lx) "
	  "empty:%p full:%p\n", this,
	  this->maxentrysize, this->qlen,
	  qesize, qesize,
	  this->empty, this->full);
  if (this->empty != NULL) {
    fprintf(stderr, "empty entries:\n");
    for (queue_entry_t tmp=this->empty; tmp!=NULL; tmp=tmp->next){
      queue_entry_dump(tmp, file);
    }
  }
  if (this->full != NULL) {
    fprintf(stderr, "full entries:\n");
    for (queue_entry_t tmp=this->empty; tmp!=NULL; tmp=tmp->next){
      queue_entry_dump(tmp, file);
    }
  }
}
