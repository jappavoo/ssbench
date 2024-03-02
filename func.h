#ifndef __SSBENCH_FUNC__
#define __SSBENCH_FUNC__

typedef void * ssbench_func(uint8_t *data, size_t n);
typedef ssbench_func * ssbench_func_t;


ssbench_func_t func_getfunc(const char *path);

#endif
