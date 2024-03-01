#ifndef __OPSERVER_H__
#define __OPSERVER_H__

// typedefs
typedef struct opserver * opserver_t;
typedef void * opserver_func(opserver_t this);

struct opserver {
  uint32_t       id;
  pthread_t      tid;
  size_t         maxmsgsize;
  int            qlen;
  opserver_func *func;
  UT_hash_handle hh;
  cpu_set_t      cpumask;
  semid_t        semid;
  struct queue   queue;
};
// public getters
static inline int opserver_getId(opserver_t this) {
  return this->id;
}
static inline opserver_func * sockserver_getFunc(opserver_t this) {
  return this->func;
}
static inline pthread_t opserver_getTid(opserver_t this) {
  return this->tid;
}

// new operator 
extern opserver_t opserver_new(uint32_t id, opserver_func *func,
			       size_t maxmsgsize, int qlen);

// core mothods
extern void opserver_start();
  
#endif
