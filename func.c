#include "ssbench.h"

typedef struct func_desc * func_desc_t;

struct func_desc {
  const char    *path;
  int            dlfd;
  ssbench_func_t func;
  func_desc_t    hh;
};

func_desc_t funchashtable;

ssbench_func_t 
func_getfunc(const char *path)
{
  return NULL;
}

