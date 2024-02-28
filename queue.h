#ifndef __QUEUE_H__
#define __QUEUE_H__

typedef struct queue_entry * queue_entry_t;

struct queue_entry  {
  queue_entry_t next;
  uint8_t data[];
};

struct queue {
  // We pad these structures to cachelines to avoid false sharing
  // I have not thought to hard about if this really makes sense
  queue_entry_t empty;              // list of entries that are empty : no data
  char pad0[CACHE_LINE_SIZE];
  queue_entry_t full;               // list of entries that are full : have data
  char pad1[CACHE_LINE_SIZE];
  struct queue_entry  entries[];
} __attribute__ ((aligned (CACHE_LINE_SIZE)));;

enum QueueEntryFindRC { Q_NONE = 0, Q_FULL=-1, Q_OK=1 };

#endif
