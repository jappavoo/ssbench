#ifndef __MSG_H__
#define __MSG_H__

union ssbench_msghdr {
  msghdr_bytes_t raw;
  char     buf[sizeof(msghdr_bytes_t)];
  struct {
    union wqpair_t wq;
    uint32_t       datalen;
  };
};
    
#endif
