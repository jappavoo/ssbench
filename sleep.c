#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <pthread.h>

#define DEFAULT_SLEEP 20

size_t func_sleep(uint8_t *data_in, size_t n_in,
		  uint8_t *data_out, size_t n_out,
		  void *ssbench_ctxt)
{
  char buf[n_in+1];
  int n;
  memcpy(buf,data_in,n_in);
  buf[n_in]=0;
  
  if (sscanf(buf, "%d", &n)!=1) n=DEFAULT_SLEEP;
  fprintf(stderr, "%ld, sleeping for %d\n", pthread_self(), n);
  sleep(n);
  fprintf(stderr, "%ld, done.\n", pthread_self());
  return 0;
}

// This funtion will get invoked once when the path is loaded
// It is NOT invoked for each input created to invoke this path
// Put any one time initialization or config code you want in this function
// it must return the ssbench func you want to a funcserver thread
// to invoke with the data of a message
void *  get_ssbench_func(const char *path, int verbosity) {
  return  func_sleep;
}
