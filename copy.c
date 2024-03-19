#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <pthread.h>

size_t func_copy(uint8_t *data_in, size_t n_in,
		 uint8_t *data_out, size_t n_out,
		 void *ssbench_ctxt)
{
  pthread_t tid =  pthread_self();
  if (data_in != NULL) {
    assert(data_out != NULL);
    assert(n_out >= n_in);
  }
  fprintf(stderr, "%ld, copy %lu bytes from %p(%lu) to %p(%lu)\n", tid,
	  n_in, data_in, n_in, data_out, n_out);
  memcpy(data_out, data_in, n_in);
  fprintf(stderr, "%ld, done.\n", tid);
  return n_in;
}

// This funtion will get invoked once when the path is loaded
// It is NOT invoked for each input created to invoke this path
// Put any one time initialization or config code you want in this function
// it must return the ssbench func you want to a funcserver thread
// to invoke with the data of a message
void *  get_ssbench_func(const char *path, int verbosity) {
  return  func_copy;
}
