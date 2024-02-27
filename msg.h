#ifndef __MSG_H__
#define __MSG_H__

union ssbench_msghdr {
  uint64_t raw;
  char     buf[sizeof(uint64_t)];
  struct {
    uint32_t opid;
    uint32_t datalen;
  } fields;
};
    
#endif
