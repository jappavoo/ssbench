#ifndef __SOCKSERVER_H__
#define __SOCKSERVER_H__

// typedefs
typedef struct sockserver * sockserver_t;

struct sockserver_msgbuffer {
  union ssbench_msghdr hdr;
  int n;
  uint8_t *data;
}; 

struct sockserver {
  PortType  port;
  FDType    listenFd;
  int       epollfd;
  int       id;
  pthread_t tid;
  struct sockserver_msgbuffer msgbuffer;
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
static inline struct sockserver_msgbuffer *
sockserver_getMsgbufferPtr(sockserver_t this) {
  return &(this->msgbuffer);
} 
static inline pthread_t sockserver_getMsgcnt(sockserver_t this) {
  return this->msgcnt;
}


// new operator 
extern sockserver_t sockserver_new(int port, int id);

// core methods
extern void sockserver_dump(sockserver_t this, FILE *file);
extern void sockserver_start(sockserver_t this, bool async);


#endif
