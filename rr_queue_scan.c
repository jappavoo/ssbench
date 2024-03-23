#include "ssbench.h"

queue_scanstate_t
rr_queue_scanstate_init(void)
{
  return (queue_scanstate_t)QID_MIN;
}

queue_scanstate_t 
rr_queue_scanfull(queue_t qs[], qid_t n, qid_t *idx, queue_entry_t *qe,
		   queue_scanstate_t ss)
{
  qid_t         qi  = (qid_t)((long int)ss);
  queue_entry_t tqe = NULL;
  queue_t       q;

  while (tqe == NULL) {
    q = qs[qi];
    queue_getFullEntry(q, &tqe);
    qi++;
    if (qi == n) qi=QID_MIN;
  }
  *qe = tqe;
  return (queue_scanstate_t)((long int)qi);
}
		   
