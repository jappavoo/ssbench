#include "ssbench.h"

void * sockserver_func(void * arg)
{
  sockserver_t this = arg;
  int fd = sockserver_getListenFd(this);
  VPRINT("%p:listenFd:%d\n", this, fd);
  // add barrier
  while (1) {
    int connfd = net_accept(fd);
    VPRINT("%p: new connection:%d\n", this, connfd);
  }
}

void
sockserver_init(sockserver_t this, int port)
{
  int rc;
  int fd;
  
  rc=net_setup_listen_socket(&fd, &port);

  if (rc==0) { 
    VLPRINT(0,"net_setup_listen_socket: fd=%d port=%d\n",
	    fd,
	    port);
    return;
  }

  sockserver_setListenFd(this, fd);
  sockserver_setPort(this, port);

 
  if (net_listen(fd) < 0) {
    VLPRINT(0, "Error: net_listen: %d:%d", fd, port);
    return;
  }
  
  if (verbose(1)) sockserver_dump(this, stderr);

}

void sockserver_start(sockserver_t this, bool async)
{
  pthread_t tid;
  if (async) pthread_create(&tid, NULL, &sockserver_func, this);
  else sockserver_func(this);
}
		 
sockserver_t
sockserver_new(int port) {
  sockserver_t this;
  this = malloc(sizeof(struct sockserver));
  assert(this);
  sockserver_init(this, port);
  return this;
}

void
sockserver_dump(sockserver_t this, FILE *file)
{
  fprintf(file, "this:%p listenFd:%d port:%d\n", this,
	  sockserver_getListenFd(this),
	  sockserver_getPort(this));
}
