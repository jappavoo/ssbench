#ifndef __MSGSERVER_H__
#define __MSGSERVER_H__

typedef struct msgserver * msgserver_t;
typedef void * msgserver_func(msgserver_t this);

struct msgserver {
  uint32_t id;
  msgserver_func *func;
  pthread_t tid;
};

extern msgserver_t msgserver_new(int id, msgserver_func *func);

#endif
