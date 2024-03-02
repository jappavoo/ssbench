#ifndef __SOCKSERVER_H__
#define __SOCKSERVER_H__
//012345678901234567890123456789012345678901234567890123456789012345678901234567
// typedefs
typedef struct sockserver * sockserver_t;
typedef struct sockserver_connection * sockserver_connection_t;
typedef struct sockserver_msgbuffer * sockserver_msgbuffer_t;

struct sockserver_msgbuffer {
  union ssbench_msghdr hdr;
  int                  n;
  queue_entry_t        qe;
}; 

struct sockserver_connection {
  struct sockaddr_storage     addr;
  socklen_t                   addrlen;
  int                         fd;
  int                         msgcnt;
  sockserver_t                ss;
  struct sockserver_msgbuffer mbuf;
};

struct sockserver {
  PortType       port;
  FDType         listenFd;
  int            epollfd;
  int            id;
  pthread_t      tid;
  int            numconn;
  int            msgcnt;
  char           name[16];
  cpu_set_t      cpumask;
  UT_hash_handle hh;
};

// public getters
static inline int sockserver_getPort(sockserver_t this) {
  return this->port;
}
static inline int sockserver_getListenFd(sockserver_t this) {
  return this->listenFd;
}
static inline int sockserver_getId(sockserver_t this) {
  return this->id;
}
static inline int sockserver_getEpollfd(sockserver_t this) {
  return this->epollfd;
}
static inline pthread_t sockserver_getTid(sockserver_t this) {
  return this->tid;
}
static inline int sockserver_getNumconn(sockserver_t this) {
  return this->numconn;
}
static inline int sockserver_getMsgcnt(sockserver_t this) {
  return this->msgcnt;
}
static inline cpu_set_t sockserver_getCpumask(sockserver_t this)
{
  return this->cpumask;
}
// new operator 
extern sockserver_t sockserver_new(int port, int id, cpu_set_t mask);

// core methods
extern void sockserver_dump(sockserver_t this, FILE *file);
extern void sockserver_start(sockserver_t this, bool async);


#endif
