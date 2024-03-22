#include "ssbench.h"

queue_scanstate_t
rr_queue_scanstate_init(void)
{
  return (queue_scanstate_t)0;
}

queue_scanstate_t 
rr_queue_scanfull(queue_t qs[], qid_t n, qid_t *idx, queue_entry_t *qe,
		   queue_scanstate_t ss)
{
  *idx = 0;
  *qe  = NULL;
  return ss;
}
		   
