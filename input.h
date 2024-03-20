#ifndef __INPUT_H__
#define __INPUT_H__
//012345678901234567890123456789012345678901234567890123456789012345678901234567
// typedefs
struct input_msgbuffer {
  union ssbench_msghdr hdr;
  int                  n;
  worker_t             worker;
  queue_entry_t        qe;
}; 

struct input_connection {
  struct sockaddr_storage addr;
  input_t                 input;
  socklen_t               addrlen;
  FDType                  fd;
  int                     msgcnt;
  struct input_msgbuffer  mbuf;
};

struct input {
  char           name[16];
  UT_hash_handle hh;
  cpu_set_t      cpumask;
  pthread_t      tid;
  PortType       port;
  FDType         listenFd;
  int            epollfd;
  int            numconn;
  int            msgcnt;
  hashid_t       id;
};

// public getters
static inline int input_getPort(input_t this) {
  return this->port;
}
static inline int input_getListenFd(input_t this) {
  return this->listenFd;
}
static inline inputid_t input_getId(input_t this) {
  return this->id;
}
static inline int input_getEpollfd(input_t this) {
  return this->epollfd;
}
static inline pthread_t input_getTid(input_t this) {
  return this->tid;
}
static inline int input_getNumconn(input_t this) {
  return this->numconn;
}
static inline int input_getMsgcnt(input_t this) {
  return this->msgcnt;
}
static inline cpu_set_t input_getCpumask(input_t this)
{
  return this->cpumask;
}
static inline char * input_getName(input_t this)
{
  return this->name;
}

static inline unsigned int input_sizeofName(input_t this)
{
  return sizeof(this->name);
}
// new operator 
extern input_t input_new(int port, inputid_t id, cpu_set_t mask);

// core methods
extern void input_dump(input_t this, FILE *file);
extern void input_start(input_t this, bool async);
extern void input_destroy(input_t this);

#endif
