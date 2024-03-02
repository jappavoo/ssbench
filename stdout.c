#include <stdio.h>
#include <stdint.h>
#include <unistd.h>

void * stdout_write(uint8_t *data, size_t n)
{
  write(STDOUT_FILENO, data, n);
  return NULL;
}

// put any initialization on config code you want in this function
// it must return the ssbench func you want to a funcserver thread
// to invoke with the data of a message
void *  get_ssbench_func(void) {
  fprintf(stderr, "stdout.so: %s : %d\n", __func__, __LINE__);
  return  stdout_write;
}
