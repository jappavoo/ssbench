#ifndef __SOCKSERVER_H__
#define __SOCKSERVER_H__

// typedefs
typedef struct sockserver * sockserver_t;

struct sockserver {
  PortType port;
  FDType listenFd;
  int    epollfd;
  int    id;
  pthread_t tid;
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

// new operator 
extern sockserver_t sockserver_new(int port, int id);

// core methods
extern void sockserver_dump(sockserver_t this, FILE *file);
extern void sockserver_start(sockserver_t this, bool async);


#endif
