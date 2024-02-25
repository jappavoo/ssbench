#ifndef __SOCKSERVER_H__
#define __SOCKSERVER_H__

struct sockserver {
  PortType port;
  FDType listenFd;
  pthread_t tid;
  
};

typedef struct sockserver * sockserver_t;

inline int sockserver_getPort(sockserver_t this) {
  return this->port;
}

inline void sockserver_setPort(sockserver_t this, int p) {
  this->port = p;
}

inline int sockserver_getListenFd(sockserver_t this) {
  return this->listenFd;
}

inline void sockserver_setListenFd(sockserver_t this, int fd) {
  this->listenFd = fd;
}

extern sockserver_t sockserver_new(int port);

// core methods
extern void sockserver_dump(sockserver_t this, FILE *file);
extern void sockserver_start(sockserver_t this, bool async);


#endif
