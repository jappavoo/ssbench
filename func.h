#ifndef __SSBENCH_FUNC__
#define __SSBENCH_FUNC__

typedef size_t ssbench_func(uint8_t *data_in, size_t n_in,
			    uint8_t *data_out, size_t n_out,
			    void *ctxt);

typedef ssbench_func * ssbench_func_t;

ssbench_func_t func_getfunc(const char *path);

#endif
