#ifndef __QUEUE_H__
#define __QUEUE_H__

typedef struct queue_entry * queue_entry_t;

struct queue_entry  {
  int len;
  char data[];
};

enum QueueEntryFindRC { Q_NONE = 0, Q_FULL=-1, Q_OK=1 };

#endif
