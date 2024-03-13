#ifndef __INPUTSERVER_H__
#define __INPUTSERVER_H__
//012345678901234567890123456789012345678901234567890123456789012345678901234567
// typedefs
typedef struct inputserver * inputserver_t;
typedef struct inputserver_connection * inputserver_connection_t;
typedef struct inputserver_msgbuffer * inputserver_msgbuffer_t;

struct inputserver_msgbuffer {
  union ssbench_msghdr hdr;
  int                  n;
  funcserver_t         fsrv;
  queue_entry_t        qe;
}; 

struct inputserver_connection {
  struct sockaddr_storage     addr;
  socklen_t                   addrlen;
  int                         fd;
  int                         msgcnt;
  inputserver_t                ss;
  struct inputserver_msgbuffer mbuf;
};

struct inputserver {
  PortType       port;
  FDType         listenFd;
  int            epollfd;
  int            id;
  int            numconn;
  int            msgcnt;
  pthread_t      tid;
  cpu_set_t      cpumask;
  char           name[16];
  UT_hash_handle hh;
};

// public getters
static inline int inputserver_getPort(inputserver_t this) {
  return this->port;
}
static inline int inputserver_getListenFd(inputserver_t this) {
  return this->listenFd;
}
static inline int inputserver_getId(inputserver_t this) {
  return this->id;
}
static inline int inputserver_getEpollfd(inputserver_t this) {
  return this->epollfd;
}
static inline pthread_t inputserver_getTid(inputserver_t this) {
  return this->tid;
}
static inline int inputserver_getNumconn(inputserver_t this) {
  return this->numconn;
}
static inline int inputserver_getMsgcnt(inputserver_t this) {
  return this->msgcnt;
}
static inline cpu_set_t inputserver_getCpumask(inputserver_t this)
{
  return this->cpumask;
}
static inline char * inputserver_getName(inputserver_t this)
{
  return this->name;
}

static inline unsigned int inputserver_sizeofName(inputserver_t this)
{
  return sizeof(this->name);
}
// new operator 
extern inputserver_t inputserver_new(int port, int id, cpu_set_t mask);

// core methods
extern void inputserver_dump(inputserver_t this, FILE *file);
extern void inputserver_start(inputserver_t this, bool async);
extern void inputserver_destroy(inputserver_t this);

#endif
