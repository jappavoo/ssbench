#ifndef __SOCKSERVER_H__
#define __SOCKSERVER_H__

// typedefs
typedef struct sockserver * sockserver_t;

struct sockserver_msgbuffer {
  union ssbench_msghdr hdr;
  int n;
  uint8_t *data;
}; 

struct sockserver_connection {
  int fd;
  struct sockserver_msgbuffer mbuf;
};

struct sockserver {
  PortType  port;
  FDType    listenFd;
  int       epollfd;
  int       id;
  pthread_t tid;
  struct sockserver_connection *connections;
  int       connmax;
  int       numconn;
  int       msgcnt;
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
static inline struct sockserver_connection *
sockserver_getConnections(sockserver_t this) {
  return this->connections;
} 
static inline int sockserver_getNumconn(sockserver_t this) {
  return this->numconn;
}
static inline int sockserver_getconnmax(sockserver_t this) {
  return this->connmax;
}
static inline int sockserver_getMsgcnt(sockserver_t this) {
  return this->msgcnt;
}

// new operator 
extern sockserver_t sockserver_new(int port, int id);

// core methods
extern void sockserver_dump(sockserver_t this, FILE *file);
extern void sockserver_start(sockserver_t this, bool async);


#endif
