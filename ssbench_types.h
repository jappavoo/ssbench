#ifndef __SSBENCH_TYPES_H__
#define __SSBENCH_TYPES_H__

// common types that are not object specific

typedef int semid_t;
typedef uint32_t id_t;
typedef uint32_t qidx_t; 
enum ID_ENUM { ID_NULL=-1 };

#define MSGHDR_NUM_64S 1
typedef uint64_t msghdr_bytes_t[MSGHDR_NUM_64S];
typedef struct funcserver * funcserver_t;
typedef struct outputserver * outputserver_t;
typedef struct inputserver * inputserver_t;
typedef struct inputserver_connection * inputserver_connection_t;
typedef struct inputserver_msgbuffer * inputserver_msgbuffer_t;

// counting semaphore operations wait and signal
extern struct sembuf semwait;
extern struct sembuf semsignal;


#endif
