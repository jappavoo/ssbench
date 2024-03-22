#ifndef __SSBENCH_TYPES_H__
#define __SSBENCH_TYPES_H__

// common types that are not object specific
typedef int          semid_t;

// all ids are signed 16 bit (2 byte values)
typedef int16_t      ssbenchid_t;
#define ID_NULL ((ssbenchid_t)-1)

// is some sense the real input id is its port number but
// we also use monotonic numbers >=0 as input id's
typedef ssbenchid_t  inputid_t;
#define INPUTID_ISVALID(iid) (iid != ID_NULL)
// arbitrary value excluding the ID_NULL
typedef ssbenchid_t  outputid_t;
#define OUTPUTID_ISVALID(oid) (oid != ID_NULL)

// arbitrary value excluding the ID_NULL
typedef ssbenchid_t  workerid_t;
#define WORKID_ISVALID(wid) (wid != ID_NULL)
// >=0 positive index into an array of queues
typedef ssbenchid_t  qid_t;
#define QID_MIN ((qid_t)0)
#define QID_MAX ((qid_t)INT16_MAX)
#define QID_ISVALID(qid) ((qid >= QID_MIN) && (qid <= QID_MAX))

// we use UT_HASH_INT so hash ids must be integer in size
typedef int hashid_t;

union wqpair_t {
  uint32_t raw;
  struct {
    workerid_t workerid;
    qid_t      qidx;
  };
};
_Static_assert(sizeof(union wqpair_t)==sizeof(uint32_t),
	       "union funcid_t bad size");


#define MSGHDR_NUM_64S 1
typedef uint64_t msghdr_bytes_t[MSGHDR_NUM_64S];
typedef struct worker * worker_t;
typedef struct output * output_t;
typedef struct input  * input_t;
typedef struct input_connection * input_connection_t;
typedef struct input_msgbuffer * input_msgbuffer_t;

// counting semaphore operations wait and signal
extern struct sembuf semwait;
extern struct sembuf semsignal;


#endif
