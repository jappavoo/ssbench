#define _GNU_SOURCE
#include "ssbench.h"
#include "hexdump.h"
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <errno.h>

//012345678901234567890123456789012345678901234567890123456789012345678901234567
#define SSVP(fmt,...) \
  VPRINT("%p:%d:%lx:" fmt, this, sockserver_getId(this), \
	 sockserver_getTid(this), __VA_ARGS__)

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

static inline void sockserver_setCpumask(sockserver_t this, cpu_set_t cpumask) {
  this->cpumask = cpumask;
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
  mb->fsrv    = NULL;
  mb->qe      = NULL;
}


static void msgbuf_dump(sockserver_msgbuffer_t mb)
{
  fprintf(stderr, "%p:hdr.raw:%lx (funcid:%x datalen:%d) n:%d fsrv:%p qe:%p\n",
	  mb, mb->hdr.raw, mb->hdr.fields.funcid, mb->hdr.fields.datalen,
	  mb->n, mb->fsrv, mb->qe);
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
  int msgcnt = c->msgcnt;
  void *ss = c->ss;
    
  if (verbose(1)) {
    fprintf(stderr,"%p:ss:%p fd:%d msgcnt:%d sa_fam:%d addrlen:%d addr:",
	    c,ss, connfd, msgcnt, addr->sa_family, addrlen);
    if (addr->sa_family == AF_INET) {
      struct sockaddr_in *iaddr = (struct sockaddr_in *)(addr);
      int iport = ntohs(iaddr->sin_port);
      union {
	uint32_t v;
	uint8_t oc[sizeof(uint32_t)];
      } a = {.v = ntohl((uint32_t)(iaddr->sin_addr.s_addr))};
      fprintf(stderr, "%u.%u.%u.%u(%02x:%02x:%02x:%02x) port:%u(%04x) ",
	      a.oc[3], a.oc[2], a.oc[1], a.oc[0],
	      a.oc[3], a.oc[2], a.oc[1], a.oc[0],
	      iport,iport);
    } else {
      for (int j=0; j<addrlen; j++) {
	int v = addr->sa_data[j];
	fprintf(stderr,":%02hx(%d)", v,v);
      }
    }
    msgbuf_dump(&c->mbuf);
  }
}

static void sockserver_cleanupConnection(sockserver_t this,
					 sockserver_connection_t co)
{
  struct epoll_event dummyev;
  int epollfd = sockserver_getEpollfd(this);
  SSVP("co:%p \n\t", co);
  sockserver_connection_dump(this, co);
  if (co->mbuf.n != 0) NYI;  // connection was lost in the middle of a message
  // for backwards compatiblity we use dummy versus NULL see bugs section
  // of man epoll_ctl
  if (epoll_ctl(epollfd, EPOLL_CTL_DEL, co->fd, &dummyev) == -1) {
    perror("epoll_ctl: EPOLL_CTL_DEL fd");
    exit(EXIT_FAILURE);
  }
  close(co->fd);
  free(co);
}

static QueueEntryFindRC_t
sockserver_findFuncServerAndQueueEntry(sockserver_t this,
				       union ssbench_msghdr *h,
				       funcserver_t *fsrv,
				       queue_entry_t *qe)
{
  funcserver_t tmpfsrv;
  assert(h);
  uint32_t fid = h->fields.funcid;
  queue_entry_t tmpqe;
  
  HASH_FIND_INT(Args.funcServers.hashtable, &fid, tmpfsrv);

  if (tmpfsrv == NULL) {
    // no server found for this id
    VLPRINT(2, "NO funcserver found for fid=%d in message.\n",fid);
    *fsrv = NULL;
    *qe   = NULL;
    return Q_NONE;
  }
  *fsrv = tmpfsrv;
  return  funcserver_getQueueEntry(tmpfsrv, h, qe);
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
	 comb->hdr.fields.funcid, comb->hdr.buf[0],
	 comb->hdr.buf[1], comb->hdr.buf[2], comb->hdr.buf[3],
	 comb->hdr.fields.datalen, comb->hdr.buf[4],
	 comb->hdr.buf[5], comb->hdr.buf[6], comb->hdr.buf[7]);
    QueueEntryFindRC_t  qerc;
    funcserver_t fsrv;
    qerc=sockserver_findFuncServerAndQueueEntry(this, &comb->hdr, &fsrv, &qe);
    if (qerc == Q_NONE) {
      VLPRINT(2, "func:%p Q_NONE could not find an funcserver?\n", this); 
      comb->qe = NULL;
    } else if (qerc == Q_MSG2BIG) {
      VLPRINT(0, "func:%p QMSG2BIG\n", this);
      comb->qe = NULL;
    } else if (qerc == Q_FULL) {
      // add code to record and signal back pressure
      VLPRINT(0, "func:%x Q_FULL no space to place message drain\n",
	      funcserver_getId(fsrv));
      comb->qe = NULL;
    } else {
      comb->qe   = qe;
      comb->fsrv = fsrv;
    }
  }
  // buffer data of message to the queue element
  qe  = comb->qe;
  assert(n>=msghdrlen);
  n -= msghdrlen;      // n = amount of message data read so far
  const int len = comb->hdr.fields.datalen;
  assert(n<=len);
  if (qe == NULL) {
    char tmpbuf[len-n];
    // got a message for a worker we can't identify so we simply
    // drain the rest of the message from the connection and
    // toss it away
    int tmpn = net_nonblocking_readn(cofd, tmpbuf, len-n);
    if (verbose(2)) {
      VLPRINT(2, "ignoring: %d bytes\n",tmpn);
      hexdump(stderr, tmpbuf, tmpn); 
    }
    n += tmpn;
  } else {
    // sizes should all have been validated already
    char *buf = qe->data;
    n += net_nonblocking_readn(cofd, buf, len-n);
  }
  comb->n = msghdrlen + n; // record how much data we have read in total
  if (n<len) return; // have not recieved all the message data yet
    
  // message has been received then release to worker
  if (qe != NULL) {
    funcserver_t fsrv = comb->fsrv;
    assert(fsrv);
    qe->len = comb->n - msghdrlen;
    assert(qe->len == len);
    if (verbose(2)) {
      VLPRINT(2, "Got msg for fsrv:%p id:%x\n", fsrv, funcserver_getId(fsrv));
      hexdump(stderr, qe->data, qe->len);
    }
    funcserver_putBackQueueEntry(fsrv, &qe);
  } else {
    if (verbose(2)) {
      VLPRINT(2,"%s", "Message skipped\n")
    }
  }

  co->msgcnt++;
  // reset connection message buffer
  msgbuf_reset(comb);
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
  char *name = sockserver_getName(this);
  struct epoll_event ev, events[MAX_EVENTS];
  
  sockserver_setTid(this, tid);
  snprintf(name,sockserver_sizeofName(this),"ss%d",
	   sockserver_getPort(this));
  pthread_setname_np(tid, name);
  
  if (verbose(1)) {
    cpu_set_t cpumask;
    assert(pthread_getaffinity_np(tid, sizeof(cpumask), &cpumask)==0);
    SSVP("%s", "cpuaffinity:");
    cpusetDump(stderr, &cpumask);
  }

  SSVP("listenConnection:%p:listenFd:%d:epollfd:%d\n",
       &listenConnection,listenConnection.fd, epollfd);

  ev.events = EPOLLIN;
  ev.data.ptr = &listenConnection;

  if (epoll_ctl(epollfd, EPOLL_CTL_ADD, listenConnection.fd, &ev) == -1) {
    perror("epoll_ctl: listenfd");
    exit(EXIT_FAILURE);
  }

  pthread_barrier_wait(&(Args.inputServers.barrier));
  
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
        SSVP("new connection:%d\n\t", connfd);
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
	  NYI;
	}
	if (evnts & EPOLLRDHUP) {
	  SSVP("connection lost: co:%p fd:%d got EPOLLRDHUP(%x) evnts:%x\n",
	       co, co->fd, EPOLLRDHUP, evnts);
	  evnts = evnts & ~EPOLLRDHUP;
	  sockserver_cleanupConnection(this, co);
	}
	if (evnts & EPOLLERR) {
	  SSVP("co:%p fd:%d got EPOLLERR(%x) evnts:%x",
	       co, co->fd, EPOLLERR, evnts);
	  evnts = evnts & ~EPOLLERR;
	  NYI;
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
sockserver_init(sockserver_t this, int port, int id, cpu_set_t cpumask)
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
  sockserver_setId(this, id);
  sockserver_setMsgcnt(this, 0);
  sockserver_setNumconn(this, 0); 
  sockserver_setCpumask(this, cpumask);
  
  if (net_listen(fd) < 0) {
    VLPRINT(0, "Error: net_listen: %d:%d", fd, port);
    return;
  }
  {
    int epollfd = epoll_create1(0);
    assert(epollfd != -1);
    sockserver_setEpollfd(this,epollfd);
  }
  
  if (verbose(2)) sockserver_dump(this, stderr);

}

void sockserver_start(sockserver_t this, bool async)
{
  pthread_t tid;
  cpu_set_t cpumask = sockserver_getCpumask(this);
  if (async) {
    pthread_attr_t attr;
    pthread_attr_t *attrp;
    
    attrp = &attr;  
    assert(pthread_attr_init(attrp)==0);
    assert(pthread_attr_setaffinity_np(attrp, sizeof(cpumask), &cpumask)==0);   
    assert(pthread_create(&tid, attrp, &sockserver_func, this)==0);
    assert(pthread_attr_destroy(attrp)==0);
  } else {
    // set and confirm affinity
    tid = pthread_self();
    assert(pthread_setaffinity_np(tid, sizeof(cpumask), &cpumask)==0);
    sockserver_func(this);
  }
}

sockserver_t
sockserver_new(int port, int id, cpu_set_t cpumask) {
  sockserver_t this;
  
  this = malloc(sizeof(struct sockserver));
  assert(this);
  sockserver_init(this, port, id, cpumask);  
  return this;
}

void
sockserver_dump(sockserver_t this, FILE *file)
{
  fprintf(file, "sockserver:%p id:%d tid:%ld listenFd:%d port:%d\n", this,
	  sockserver_getId(this),
	  sockserver_getTid(this),
	  sockserver_getListenFd(this),
	  sockserver_getPort(this));
}
