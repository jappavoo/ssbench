#ifndef __MSG_H__
#define __MSG_H__

struct msg {
  union {
    uint64_t raw;
    struct {
      uint32_t opid;
      uint32_t datalen;
    } info;
  } hdr;
  char data[];
};
    
#endif
