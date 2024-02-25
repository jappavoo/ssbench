#include "ssbench.h"
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>

#define SSVP(fmt,...) VPRINT("%p:%d:%lx:" fmt, this, this->id, this->tid, __VA_ARGS__)

static inline void sockserver_setPort(sockserver_t this, int p) {
  this->port = p;
}

static inline void sockserver_setListenFd(sockserver_t this, int fd) {
  this->listenFd = fd;
}

static inline void sockserver_setId(sockserver_t this, int id) {
  this->id = id;
}

static inline void sockserver_setEpollfd(sockserver_t this, int fd) {
  this->epollfd = fd;
}

static inline void sockserver_setTid(sockserver_t this, pthread_t tid) {
  this->tid = tid;
}

static inline void sockserver_setMsgcnt(sockserver_t this, int c) {
  this->msgcnt = c;
}

static inline void sockserver_setNumconn(sockserver_t this, int n) {
  this->numconn = n;
}

static inline void sockserver_incNumconn(sockserver_t this) {
  this->numconn++;
}

static void setnonblocking(int fd)
{
  int flags;
  
  flags = fcntl(fd, F_GETFD);
  assert(flags!=-1);
  flags |= O_NONBLOCK;
  flags = fcntl(fd, F_SETFD);  
}

static void
sockserver_connection_dump(sockserver_t this, struct sockserver_connection *c) 
{
  int connfd = c->fd;
  struct sockaddr *addr = (struct sockaddr *)&(c->addr);
  socklen_t addrlen = c->addrlen;
  
  if (verbose(1)) {
    SSVP("new connection: %d sa_fam:%d len:%d\n",  connfd, addr->sa_family, addrlen);
    if (addr->sa_family == AF_INET) {
      struct sockaddr_in *iaddr = (struct sockaddr_in *)(addr);
      int iport = ntohs(iaddr->sin_port);
      union {
	uint32_t v;
	uint8_t oc[sizeof(uint32_t)];
      } a = {.v = ntohl((uint32_t)(iaddr->sin_addr.s_addr))};
      fprintf(stderr, "addr:%u.%u.%u.%u (%02x:%02x:%02x:%02x) port:%u (%04x)\n",
	      a.oc[3], a.oc[2], a.oc[1], a.oc[0],
	      a.oc[3], a.oc[2], a.oc[1], a.oc[0],
	      iport,iport);
    } else {
      fprintf(stderr, "addr->sa_data");
      for (int j=0; j<addrlen; j++) {
	int v = addr->sa_data[j];
	fprintf(stderr,":%02hx(%d)", v,v);
      }
    }
  }
}

static void sockserver_bufferMsg(sockserver_t this, int fd)
{
  struct sockserver_msgbuffer msgbuffer;
  SSVP("%d\n", fd);
}

#define MAX_EVENTS 1024
// epoll code is based on example from the man page
static void * sockserver_func(void * arg)
{
  sockserver_t this = arg;
  int listenfd = sockserver_getListenFd(this);
  int id = sockserver_getId(this);
  int epollfd = sockserver_getEpollfd(this);
  pthread_t tid = pthread_self();
  struct epoll_event ev, events[MAX_EVENTS];

  sockserver_setTid(this, tid);  
  SSVP("listenFd:%d:epollfd:%d\n", listenfd, epollfd);

  ev.events = EPOLLIN;
  ev.data.fd = listenfd;

  if (epoll_ctl(epollfd, EPOLL_CTL_ADD, listenfd, &ev) == -1) {
    perror("epoll_ctl: listenfd");
    exit(EXIT_FAILURE);
  }

  pthread_barrier_wait(&(Args.socketServers.barrier));
  
  for (;;) {
    int nfds = epoll_wait(epollfd, events, MAX_EVENTS, -1);
    if (nfds == -1) {
      perror("epoll_wait");
      exit(EXIT_FAILURE);
    }
    
    for (int n = 0; n < nfds; ++n) {
      if (events[n].data.fd == listenfd) {
	struct sockaddr_storage addr;
	// addrlen must be initilized to size of addr in bytes.  It will be
	// updated with lenght of peer address (see man accept). If you don't do this
	// you will not get a valid address sent back
 	socklen_t addrlen = sizeof(struct sockaddr_storage); 
	int  connfd = net_accept(listenfd, &addr, &addrlen);
	if (connfd == -1) {
	  perror("accept");
	  exit(EXIT_FAILURE);
	}
	setnonblocking(connfd);
	// create a new connection object for this connection
	struct sockserver_connection *co = malloc(sizeof(struct sockserver_connection));
	co->addr         = addr;
	co->addrlen      = addrlen;
	co->fd           = connfd;
	co->msgcnt       = 0;
	co->mbuf.hdr.raw = 0;
	co->mbuf.n       = 0;
	co->mbuf.data    = NULL;
	co->ss           = this;
	
	sockserver_incNumconn(this);
	sockserver_connection_dump(this, co);
	
	ev.events  = EPOLLIN | EPOLLET;
	ev.data.fd = connfd;
	ev.data.ptr     = co;
	
	if (epoll_ctl(epollfd, EPOLL_CTL_ADD, connfd,
		      &ev) == -1) {
	  perror("epoll_ctl: connfd");
	  exit(EXIT_FAILURE);
	}
      } else {
	int msgfd = events[n].data.fd;
	SSVP("activity on:%d\n", msgfd);
	sockserver_bufferMsg(this, msgfd);
      }
    }		   
  }
}

static void
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

  // we are using epoll so we set the listen fd to non-blocking mode
  setnonblocking(fd);
  
  sockserver_setListenFd(this, fd);
  sockserver_setPort(this, port);

  if (net_listen(fd) < 0) {
    VLPRINT(0, "Error: net_listen: %d:%d", fd, port);
    return;
  }
  {
    int epollfd = epoll_create1(0);
    assert(epollfd != -1);
    sockserver_setEpollfd(this,epollfd);
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
sockserver_new(int port, int id) {
  sockserver_t this;
  struct sockserver_connection *connections;
  
  this = malloc(sizeof(struct sockserver));
  assert(this);
  sockserver_init(this, port);
  sockserver_setId(this, id);
  sockserver_setMsgcnt(this, 0);
  sockserver_setNumconn(this, 0); 
  
  return this;
}

void
sockserver_dump(sockserver_t this, FILE *file)
{
  fprintf(file, "this:%p id:%d tid:%ld listenFd:%d port:%d\n", this,
	  sockserver_getId(this),
	  sockserver_getTid(this),
	  sockserver_getListenFd(this),
	  sockserver_getPort(this));
}
