#include "ssbench.h"

void * stdout_write(uint8_t *data, size_t n)
{
  write(STDOUT_FILENO, data, n);
  return NULL;
}

// put any initialization on config code you want in this function
// it must return the ssbench func you want to a funcserver thread
// to invoke with the data of a message
ssbench_func_t get_func() {
 return  stdout_write;
}
