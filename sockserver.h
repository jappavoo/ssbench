#ifndef __SOCKSERVER_H__
#define __SOCKSERVER_H__

struct sockserver {
  int port;
};

typedef struct sockserver * sockserver_t;

inline int sockserver_getPort(sockserver_t this) {
  return this->port;
}

inline void sockserver_setPort(sockserver_t this, int p) {
  this->port = p;
}

extern sockserver_t sockserver_new(int port);
extern void sockserver_dump(sockserver_t this, FILE *file);

#endif
