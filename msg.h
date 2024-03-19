#ifndef __MSG_H__
#define __MSG_H__

union ssbench_msghdr {
  msghdr_bytes_t raw;
  char     buf[sizeof(msghdr_bytes_t)];
  struct {
    id_t funcid;
    // qidx_t qidx
    uint32_t datalen;
    // uint32_t unused;
  } fields;
};
    
#endif
