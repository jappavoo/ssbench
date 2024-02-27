#include "ssbench.h"
#include "hexdump.h"
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <errno.h>

//012345678901234567890123456789012345678901234567890123456789012345678901234567
#define SSVP(fmt,...) \
  VPRINT("%p:%d:%lx:" fmt, this, this->id, this->tid, __VA_ARGS__)

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

static inline void sockserver_incMsgcnt(sockserver_t this, int c) {
  this->msgcnt++;
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

static void msgbuf_reset(sockserver_msgbuffer_t mb)
{
  mb->hdr.raw = 0;
  mb->n       = 0;
  mb->qe      = NULL;
}

static sockserver_connection_t
sockserver_connection_new(struct sockaddr_storage addr, socklen_t addrlen,
			  int fd, sockserver_t ss)
{
  	sockserver_connection_t co =
	  malloc(sizeof(struct sockserver_connection));
	co->addr         = addr;
	co->addrlen      = addrlen;
	co->fd           = fd;
	co->msgcnt       = 0;
	msgbuf_reset(&co->mbuf);
	co->ss           = ss;
	return co;
}

static void
sockserver_connection_dump(sockserver_t this, struct sockserver_connection *c) 
{
  int connfd = c->fd;
  struct sockaddr *addr = (struct sockaddr *)&(c->addr);
  socklen_t addrlen = c->addrlen;
  
  if (verbose(1)) {
    SSVP("new connection: %d sa_fam:%d len:%d\n",
	 connfd, addr->sa_family, addrlen);
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

static enum QueueEntryFindRC
sockserver_findOpServerQueueEntry(sockserver_t this, union ssbench_msghdr *h,
				  queue_entry_t *qe)
{
  *qe = NULL;
  return Q_NONE;
}  


static void sockserver_serveConnection(sockserver_t this,
				       sockserver_connection_t co)
{
  // We got data on the fd our job is to buffer a message to a work
  // and then let the work processes it.  Given that a socket is a serial
  // stream of bytes we will assume that messages are contigous and that
  // as single message is destined to a single worker.  A future extention
  // would be to support broadcast and multicast but that is not something
  // we are supporting now.

  // Each connection object contains the state of the current message begin
  // buffered.  Once a message is released to a work the connections message
  // buffer needs to be reset.
  int cofd = co->fd;
  struct sockserver_msgbuffer *comb = &co->mbuf;
  const int msghdrlen = sizeof(union ssbench_msghdr);
  int n = comb->n; 
  queue_entry_t qe;
  char *buf;
  
  SSVP("co:%p mb.n=%d\n",co,comb->n);
  if ( n < msghdrlen) {
    assert(n<=msghdrlen);
    n += net_nonblocking_readn(cofd, &comb->hdr.buf[n], msghdrlen-n);
    comb->n = n;
    if (n<msghdrlen) return; // have not received full msg hdr yet
    // We have a whole message header. Switch over to buffering
    // the data of the message to the correct operator queue
    SSVP("msghdr: opid:%x(%02hhx %02hhx %02hhx %02hhx) "
	 "datalen:%u (%02hhx %02hhx %02hhx %02hhx)\n",
	 comb->hdr.fields.opid, comb->hdr.buf[0],
	 comb->hdr.buf[1], comb->hdr.buf[2], comb->hdr.buf[3],
	 comb->hdr.fields.datalen, comb->hdr.buf[4],
	 comb->hdr.buf[5], comb->hdr.buf[6], comb->hdr.buf[7]);
    enum QueueEntryFindRC  qerc;
    qerc=sockserver_findOpServerQueueEntry(this, &comb->hdr, &qe);
    if (qerc == Q_NONE) {
      comb->qe = NULL;
    } else if (qerc == Q_FULL) {
      comb->qe = NULL;
    } else {
      comb->qe = qe;
    }
  }
  // buffer data of message to the worker
  qe  = comb->qe;
  assert(n>=msghdrlen);
  n -= msghdrlen;      // n = amount of message data read so far
  const int len = comb->hdr.fields.datalen;
  if (qe == NULL) {
    assert(n<=len);
    char buf[len-n];
    // got a message for a worker we can't identify so we simply
    // drain the rest of the message from the connection and
    // toss it away
    n += net_nonblocking_readn(cofd, buf, len-n);
    if (verbose(1)) {
      hexdump(stderr, buf, n);
    }
    if (n<len) return; // have not received all the message data yet
  } else {
    NYI;
  }
  // message has been received then release to worker

  co->msgcnt++;
  // reset connection message buffer
  msgbuf_reset(comb);
  comb->qe      = NULL;
  comb->n       = 0;
  comb->hdr.raw = 0;
}

#define MAX_EVENTS 1024
// epoll code is based on example from the man page
static void * sockserver_func(void * arg)
{
  sockserver_t this = arg;
  // dummy connection object to use as
  // an epoll handle could have used &listenfd
  // but decided this looks better ;-)
  struct {
    int fd;
  } listenConnection = {
    .fd =  sockserver_getListenFd(this)
  };
  int id = sockserver_getId(this);
  int epollfd = sockserver_getEpollfd(this);
  pthread_t tid = pthread_self();
  struct epoll_event ev, events[MAX_EVENTS];
  
  sockserver_setTid(this, tid);  
  SSVP("listenConnection:%p:listenFd:%d:epollfd:%d\n",
       &listenConnection,listenConnection.fd, epollfd);

  ev.events = EPOLLIN;
  ev.data.ptr = &listenConnection;

  if (epoll_ctl(epollfd, EPOLL_CTL_ADD, listenConnection.fd, &ev) == -1) {
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
      void * activeConnection = events[n].data.ptr;
      SSVP("activeConnection:%p\n", activeConnection);
      if ( activeConnection == &listenConnection ) {
	struct sockaddr_storage addr; // in/out parameter
	// addrlen must be initilized to size of addr in bytes.  It will be
	// updated with length of the peer address (see man accept). If you don't
	// do this you will not get a valid address sent back
 	socklen_t addrlen = sizeof(struct sockaddr_storage); // out 
	int  connfd = net_accept(listenConnection.fd, &addr, &addrlen);
	if (connfd == -1) {
	  perror("accept");
	  exit(EXIT_FAILURE);
	}
	// epoll man page recommends non-blocking io for edgetriggered use 
	setnonblocking(connfd);
	// create a new connection object for this connection
	sockserver_connection_t co = sockserver_connection_new(addr, addrlen,
							       connfd, this);
	sockserver_incNumconn(this);
	sockserver_connection_dump(this, co);

	// setup event structure
	ev.events   = EPOLLIN | EPOLLET | EPOLLHUP | EPOLLRDHUP | EPOLLERR;
	ev.data.ptr = co;
	
	if (epoll_ctl(epollfd, EPOLL_CTL_ADD, connfd, &ev) == -1) {
	  perror("epoll_ctl: failed to add connfd");
	  exit(EXIT_FAILURE);
	}
      } else {
	sockserver_connection_t co = activeConnection;
	uint32_t evnts = events[n].events;
	SSVP("activity on connection:%p: events0x%x fd:%d\n",
	     co, evnts, co->fd);
	// process each of the events that have occurred on this connection
	if (evnts & EPOLLIN) {
	  SSVP("co:%p fd:%d got EPOLLIN(%x) evnts:%x\n",
	       co, co->fd, EPOLLIN, evnts);
	  sockserver_serveConnection(this, co);
	  evnts = evnts & ~EPOLLIN;
	}
	if (evnts & EPOLLHUP) {
	  SSVP("co:%p fd:%d got EPOLLHUP(%x) evnts:%x\n",
	       co, co->fd, EPOLLHUP, evnts);
	  evnts = evnts & ~EPOLLHUP;
	}
	if (evnts & EPOLLRDHUP) {
	  SSVP("co:%p fd:%d got EPOLLRDHUP(%x) evnts:%x",
	       co, co->fd, EPOLLRDHUP, evnts);
	  evnts = evnts & ~EPOLLRDHUP;
	}
	if (evnts & EPOLLERR) {
	  SSVP("co:%p fd:%d got EPOLLERR(%x) evnts:%x",
	       co, co->fd, EPOLLERR, evnts);
	  evnts = evnts & ~EPOLLERR;
	}
	if (evnts != 0) {
	  SSVP("co:%p fd:%d unknown events evnts:%x",
	       co, co->fd, evnts);
	  assert(evnts==0);
	}
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
