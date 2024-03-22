#ifndef __QUEUE_H__
#define __QUEUE_H__

typedef struct queue_desc * queue_desc_t;
typedef struct queue_entry * queue_entry_t;
typedef struct queue *queue_t;
typedef void * queue_scanstate_t;

typedef queue_scanstate_t queue_scanstate_init_func(void);
typedef queue_scanstate_t queue_scanfull_func(queue_t queues[],
					      qid_t   n,
					      qid_t   *qidx,
					      queue_entry_t *qe,
					      queue_scanstate_t ss);
typedef queue_scanstate_init_func * queue_scanstate_init_func_t;
typedef queue_scanfull_func * queue_scanfull_func_t;

struct queue_desc {  
  size_t maxentrysize;
  size_t qlen;
  qid_t  count;
};

struct queue_entry  {
  queue_entry_t next;
  size_t  len;
  uint8_t data[];
};

struct queue {
  // We pad these structures to cachelines to avoid false sharing
  // I have not thought to hard about if this really makes sense
  volatile queue_entry_t empty;     // list of entries that are empty : no data
  char pad0[CACHE_LINE_SIZE];
  volatile queue_entry_t full;      // list of entries that are full : have data
  char pad1[CACHE_LINE_SIZE];
  size_t                 maxentrysize;
  size_t                 qlen;
  struct queue_entry     entries[];
} __attribute__ ((aligned (CACHE_LINE_SIZE)));;

typedef enum  {
  Q_NONE     =  0,
  Q_BADQIDX  = -1,
  Q_FULL     = -2,
  Q_MSG2BIG  = -3,
  Q_FOUND    =  1
} QueueEntryFindRC_t;

static inline size_t queue_getQlen(queue_t this) { return this->qlen; }
static inline size_t queue_getMaxentrysize(queue_t this) {
  return this->maxentrysize;
}
static inline size_t queue_entrysize(queue_t this)
{
  return (queue_getMaxentrysize(this) + sizeof(struct queue_entry));
}
static inline queue_entry_t queue_getentryi(queue_t this, int i)
{
  uint8_t *ptr = (uint8_t *)(this->entries);
  return (queue_entry_t)(ptr + (queue_entrysize(this) * i));
}

__attribute__((unused))
static QueueEntryFindRC_t
queue_getEmptyEntry(queue_t this, bool checklen, size_t *len, queue_entry_t *qe)
{
  void * volatile *empty_ptr = (void * volatile *)&(this->empty);
  if (checklen) {
    if (*len > queue_getMaxentrysize(this)) {
      *qe = NULL;
      return Q_MSG2BIG;
    }
  } else {
    *len = queue_getMaxentrysize(this);
  }
 retry:
  queue_entry_t qe_head = this->empty;
  if (qe_head == NULL) {
    *qe = NULL;
    return Q_FULL;
  }
  queue_entry_t qe_next = qe_head->next;
  if (!__sync_bool_compare_and_swap(empty_ptr,
				    (void *)qe_head,
				    (void *)qe_next)) goto retry;
  // disconnect the node from free list 
  qe_head->next = NULL;
  *qe = qe_head;
  return Q_FOUND;
}

__attribute__((unused)) static bool
queue_getFullEntry(queue_t this, queue_entry_t *qe)
{
  void * volatile *empty_ptr = (void * volatile *)&(this->full);

 retry:
  queue_entry_t qe_head = this->full;
  if (qe_head == NULL) {
    *qe = NULL;
    return false;
  }
  queue_entry_t qe_next = qe_head->next;
  if (!__sync_bool_compare_and_swap(empty_ptr,
				    (void *)qe_head,
				    (void *)qe_next)) goto retry;
  // disconnect the node from free list 
  qe_head->next = NULL;
  *qe = qe_head;
  if (qe_next == NULL) return true;  // true full list became empty
  else return false;;
}

// put a full entry on to the full list (must have originally been
// obtained from empty list. Returns true if this put is the first
// element to go on to the full list.  This can be used to wakeup
// a reaper thread
__attribute__((unused))
static bool queue_putBackFullEntry(queue_t this, queue_entry_t qe)
{
  void * volatile *full_ptr = (void * volatile *)&(this->full);
  
 retry:
  queue_entry_t qe_head = this->full;
  qe->next = qe_head;
  if (!__sync_bool_compare_and_swap(full_ptr,
				    (void *)qe_head,
				    (void *)qe)) goto retry;
  if (qe_head == NULL) return true; // true full list became non-empty
  return false;  
}

__attribute__((unused))
static bool queue_putBackEmptyEntry(queue_t this, queue_entry_t qe)
{
  void * volatile *empty_ptr = (void * volatile *)&(this->empty);
  qe->len = 0;
  
 retry:
  queue_entry_t qe_head = this->empty;
  qe->next = qe_head;
  if (!__sync_bool_compare_and_swap(empty_ptr,
				    (void *)qe_head,
				    (void *)qe)) goto retry;
  if (qe_head == NULL) return true;  // true empty list became non-empty
  return false;  
}

extern void queue_init(queue_t this, size_t maxentrysize, size_t qlen);
extern void queue_dump(queue_t this, FILE *file);
extern void queue_desc_dump(FILE *file, queue_desc_t qds, qid_t n);

#endif
