#ifndef __OUTPUT_H__
#define __OUTPUT_H__
//012345678901234567890123456789012345678901234567890123456789012345678901234567
// typedefs

struct output {
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
  int            msgcnt;
  hashid_t       id;
  // this field must be last
  struct queue   queue;
};

// public getters
static inline int output_getPort(output_t this) {
  return this->port;
}
static inline int output_getSendfd(output_t this) {
  return this->sendFd;
}
static inline outputid_t output_getId(output_t this) {
  return this->id;
}
static inline pthread_t output_getTid(output_t this) {
  return this->tid;
}
static inline const char * output_getHost(output_t this) {
  return this->host;
}
static inline int output_getMsgcnt(output_t this) {
  return this->msgcnt;
}
static inline cpu_set_t output_getCpumask(output_t this)
{
  return this->cpumask;
}
static inline size_t output_getMaxmsgsize(output_t this) {
  return this->maxmsgsize;
}
static inline size_t output_getQlen(output_t this) {
  return this->qlen;
}
static inline char * output_getName(output_t this) {
  return this->name;
}
static inline queue_t output_getQueue(output_t this) {
  return &(this->queue);
}
static inline unsigned int output_sizeofName(output_t this) {
  return sizeof(this->name);
}
static inline semid_t output_getSemid(output_t this) {
  return this->semid;
}

__attribute__((unused)) static QueueEntryFindRC_t
output_getQueueEntry(output_t this, size_t *len, queue_entry_t *qe)
{
  queue_t q = output_getQueue(this);
  return queue_getEmptyEntry(q, false, len, qe);
}

__attribute__((unused)) static void
output_putBackQueueEntry(output_t this, queue_entry_t *qe)
{
  queue_t q = output_getQueue(this);
  semid_t semid = output_getSemid(this);
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
extern output_t output_new(outputid_t id, const char *host, int port,
			   int sfd, size_t maxmsgsize, size_t qlen,
			   cpu_set_t mask);

// core methods
extern void output_dump(output_t this, FILE *file);
extern void output_start(output_t this, bool async);
extern void output_destroy(output_t this);

#endif
