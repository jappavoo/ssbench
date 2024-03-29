#ifndef __WORKER_H__
#define __WORKER_H__

// typedefs

struct worker {
  char            name[16];
  const char     *path;
  ssbench_func_t  func;
  output_t        output;
  UT_hash_handle  hh;
  cpu_set_t       cpumask;
  pthread_t       tid;
  semid_t         semid;
  hashid_t        id;
  outputid_t      oid;
  workerid_t      owid;
  qid_t           oqid;
  qid_t           numqs;
  // this must the last field 
  queue_t         queues[];
};
// public getters
static inline workerid_t worker_getId(worker_t this) {
  return this->id;
}
static inline outputid_t worker_getOid(worker_t this) {
  return this->oid;
}
static inline workerid_t worker_getOwid(worker_t this) {
  return this->owid;
}
static inline qid_t worker_getOqid(worker_t this) {
  return this->oqid;
}
static inline output_t worker_getOutput(worker_t this) {
  return this->output;
}
static inline ssbench_func_t worker_getFunc(worker_t this) {
  return this->func;
}
static inline pthread_t worker_getTid(worker_t this) {
  return this->tid;
}

static inline const char * worker_getPath(worker_t this) {
  return this->path;
}
static inline semid_t worker_getSemid(worker_t this) {
  return this->semid;
}
static inline cpu_set_t worker_getCpumask(worker_t this) {
  return this->cpumask;
}
static inline qid_t worker_getNumqs(worker_t this) {
  return this->numqs;
}
static inline queue_t * worker_getQueues(worker_t this)
{
  return this->queues;
}
static inline queue_t worker_getQueue(worker_t this, qid_t i) {
  return this->queues[i];
}
static inline char * worker_getName(worker_t this) {
  return this->name;
}
static inline unsigned int worker_sizeofName(worker_t this) {
  return sizeof(this->name);
}

// new func server 
extern worker_t worker_new(workerid_t id, const char * path,
			   ssbench_func_t func,
			   queue_desc_t qds, qid_t qdcount,
			   outputid_t oid, workerid_t owid, qid_t oqid,
			   cpu_set_t cpumask);

// core moth+ods
extern void worker_dump(worker_t this, FILE *file);
extern QueueEntryFindRC_t worker_getQueueEntry(worker_t this,
					       union ssbench_msghdr *h,
					       queue_entry_t *qe);
extern void worker_putBackQueueEntry(worker_t this,
				     union ssbench_msghdr *h,
				     queue_entry_t *qe);

extern void worker_start(worker_t this, bool async);
extern void worker_destroy(worker_t this);

#endif
