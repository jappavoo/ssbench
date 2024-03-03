#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <assert.h>

void * stdout_write(uint8_t *data_in, size_t n_in,
		    uint8_t *data_out, size_t n_out,
		    int verbosity,
		    void *ssbench_ctxt)
{
  write(STDOUT_FILENO, data_in, n_in);
  return NULL;
}

// This funtion will get invoked once when the path is loaded
// It is NOT invoked for each input created to invoke this path
// Put any one time initialization or config code you want in this function
// it must return the ssbench func you want to a funcserver thread
// to invoke with the data of a message
void *  get_ssbench_func(const char *path, int verbosity) {
  if (verbosity>1) {
    assert(path);
    fprintf(stderr, "%s:%s:%d:returning func stdout_write=%p\n",
	    path, __func__, __LINE__, stdout_write);
  }
  // the simple version would just be this loine
  return  stdout_write;
}
