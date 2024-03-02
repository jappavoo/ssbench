#ifndef __FUNCSERVER_H__
#define __FUNCSERVER_H__

// typedefs
typedef struct funcserver * funcserver_t;

struct funcserver {
  uint32_t        id;
  pthread_t       tid;
  size_t          maxmsgsize;
  int             qlen;
  const char     *path;
  ssbench_func_t  func;
  cpu_set_t       cpumask;
  semid_t         semid;
  UT_hash_handle  hh;
  // this must the last field 
  struct queue    queue;
};
// public getters
static inline int funcserver_getId(funcserver_t this) {
  return this->id;
}
static inline ssbench_func_t funcserver_getFunc(funcserver_t this) {
  return this->func;
}
static inline pthread_t funcserver_getTid(funcserver_t this) {
  return this->tid;
}
static inline size_t funcserver_getMaxmsgsize(funcserver_t this) {
  return this->maxmsgsize;
}
static inline size_t funcserver_getQlen(funcserver_t this) {
  return this->qlen;
}
static inline const char * funcserver_getPath(funcserver_t this) {
  return this->path;
}
static inline semid_t funcserver_getSemid(funcserver_t this) {
  return this->semid;
}
static inline cpu_set_t * funcserver_getCpumask(funcserver_t this) {
  return &(this->cpumask);
}
static inline queue_t funcserver_getQueue(funcserver_t this) {
  return &(this->queue);
}
// new func server 
extern funcserver_t funcserver_new(uint32_t id, const char * path,
				   ssbench_func_t func,
				   size_t maxmsgsize, int qlen,
				   cpu_set_t cpumask);

// core mothods
extern void funcserver_start();
extern void funcserver_dump(funcserver_t this, FILE *file);  
#endif
