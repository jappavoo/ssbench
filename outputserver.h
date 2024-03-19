#ifndef __OUTPUTSERVER_H__
#define __OUTPUTSERVER_H__
//012345678901234567890123456789012345678901234567890123456789012345678901234567
// typedefs

struct outputserver {
  char           name[16];
  const char     *host;
  pthread_t      tid;
  cpu_set_t      cpumask;
  UT_hash_handle hh;
  size_t         maxmsgsize;
  size_t         qlen;
  PortType       port;
  FDType         sendFd;
  semid_t        semid;
  int            id;
  int            msgcnt;
  // this field must be last
  struct queue   queue;
};

// public getters
static inline int outputserver_getPort(outputserver_t this) {
  return this->port;
}
static inline int outputserver_getSendfd(outputserver_t this) {
  return this->sendFd;
}
static inline int outputserver_getId(outputserver_t this) {
  return this->id;
}
static inline pthread_t outputserver_getTid(outputserver_t this) {
  return this->tid;
}
static inline const char * outputserver_getHost(outputserver_t this) {
  return this->host;
}
static inline int outputserver_getMsgcnt(outputserver_t this) {
  return this->msgcnt;
}
static inline cpu_set_t outputserver_getCpumask(outputserver_t this)
{
  return this->cpumask;
}
static inline size_t outputserver_getMaxmsgsize(outputserver_t this) {
  return this->maxmsgsize;
}
static inline size_t outputserver_getQlen(outputserver_t this) {
  return this->qlen;
}
static inline char * outputserver_getName(outputserver_t this) {
  return this->name;
}
static inline queue_t outputserver_getQueue(outputserver_t this) {
  return &(this->queue);
}
static inline unsigned int outputserver_sizeofName(outputserver_t this) {
  return sizeof(this->name);
}
static inline semid_t outputserver_getSemid(outputserver_t this) {
  return this->semid;
}

__attribute__((unused)) static QueueEntryFindRC_t
outputserver_getQueueEntry(outputserver_t this, size_t *len, queue_entry_t *qe)
{
  queue_t q = outputserver_getQueue(this);
  return queue_getEmptyEntry(q, false, len, qe);
}

__attribute__((unused)) static void
outputserver_putBackQueueEntry(outputserver_t this, queue_entry_t *qe)
{
  queue_t q = outputserver_getQueue(this);
  semid_t semid = outputserver_getSemid(this);
  queue_putBackFullEntry(q, *qe);
  *qe = NULL;
 retry:
  int rc = semop(semid, &semsignal, 1);
  if (rc<0) {
    if (errno == EINTR) goto retry;
    perror("semop:");
    EEXIT();
  }
}
// new operator 
extern outputserver_t outputserver_new(int id, const char *host, int port,
				       int sfd, size_t maxmsgsize, size_t qlen,
				       cpu_set_t mask);

// core methods
extern void outputserver_dump(outputserver_t this, FILE *file);
extern void outputserver_start(outputserver_t this, bool async);
extern void outputserver_destroy(outputserver_t this);

#endif
