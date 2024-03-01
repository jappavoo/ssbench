#ifndef __FUNCSERVER_H__
#define __FUNCSERVER_H__

// typedefs
typedef struct funcserver * funcserver_t;
typedef void * funcserver_func(funcserver_t this, void *);

struct funcserver {
  uint32_t       id;
  pthread_t      tid;
  size_t         maxmsgsize;
  int            qlen;
  char          *path;
  funcserver_func *func;
  UT_hash_handle hh;
  cpu_set_t      cpumask;
  semid_t        semid;
  struct queue   queue;
};
// public getters
static inline int funcserver_getId(funcserver_t this) {
  return this->id;
}
static inline funcserver_func * sockserver_getFunc(funcserver_t this) {
  return this->func;
}
static inline pthread_t funcserver_getTid(funcserver_t this) {
  return this->tid;
}

// new func server 
extern funcserver_t funcserver_new(uint32_t id, funcserver_func *func,
			       size_t maxmsgsize, int qlen);

// core mothods
extern void funcserver_start();
  
#endif
