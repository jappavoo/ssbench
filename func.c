#include "ssbench.h"
#include <dlfcn.h>

//012345678901234567890123456789012345678901234567890123456789012345678901234567
typedef struct func_desc * func_desc_t;

struct func_desc {
  const char    *path;
  void          *dlhdl;
  ssbench_func_t func;
  UT_hash_handle hh;
};

static func_desc_t funchashtable;

ssbench_func_t 
func_getfunc(const char *path)
{
  func_desc_t fdesc;
  ssbench_func_t (*getfunc)(const char *path,int verbosity);
  
  if (path==NULL) return NULL;
  
  HASH_FIND_STR(funchashtable, path, fdesc);
  if (fdesc == NULL) {
    void * dlhdl = dlopen(path,
			  RTLD_NOW | //  avoid costs later
			  RTLD_LOCAL); // keep symbols local other funcs can use
    // the following flag does not seem to work but since we use our
    // own hashtable it does not matter.
    //			  RTLD_NOLOAD); // don't load twice
    if (dlhdl == NULL) {
      VLPRINT(2, "dlopen failed %s for %s returing NULL for func\n",
	      dlerror(), path); 
      return NULL;
    }
    
    getfunc = dlsym(dlhdl, "get_ssbench_func");
    if (getfunc == NULL) {
      EPRINT("ERROR: was not able to find get_ssbench_func in %s\n", path);
      dlclose(dlhdl);
      exit(EXIT_FAILURE);
    }
    ssbench_func_t func = getfunc(path, Args.verbose);
    VLPRINT(2, "getfunc:%p returned %p\n", getfunc, func);
    
    fdesc = malloc(sizeof(struct func_desc));
    fdesc->path  = path;
    fdesc->dlhdl = dlhdl;
    fdesc->func  = func;
    
    HASH_ADD_KEYPTR(hh, funchashtable, fdesc->path, strlen(fdesc->path),fdesc);
    VLPRINT(2, "ADD func_desc:%p path:%s dlhdl:%p func:%p\n", fdesc,
	    fdesc->path, fdesc->dlhdl, fdesc->func);
  } else {
    VLPRINT(2, "FOUND func_desc:%p path:%s dlhdl:%p func:%p\n", fdesc,
	    fdesc->path, fdesc->dlhdl, fdesc->func);
  }
  assert(fdesc!=NULL);
  return fdesc->func;
}

