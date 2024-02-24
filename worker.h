#ifndef __WORKER_H__
#define __WORKER_H__

typedef struct worker * worker_t;
typedef void * worker_func(worker_t this);

struct worker {
  uint32_t id;
  worker_func *func;
};

extern worker_t worker_new(int id, worker_func *func);

#endif
