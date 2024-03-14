#define _GNU_SOURCE
#include "ssbench.h"
#include "hexdump.h"
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <errno.h>

//012345678901234567890123456789012345678901234567890123456789012345678901234567
#define SSVLP(VL,fmt,...)				  \
  VLPRINT(VL,"%p:%d:%lx:" fmt, this, inputserver_getId(this),	\
	 inputserver_getTid(this), __VA_ARGS__)

static inline void inputserver_setPort(inputserver_t this, int p) {
  this->port = p;
}

static inline void inputserver_setListenFd(inputserver_t this, int fd) {
  this->listenFd = fd;
}

static inline void inputserver_setId(inputserver_t this, int id) {
  this->id = id;
}

static inline void inputserver_setEpollfd(inputserver_t this, int fd) {
  this->epollfd = fd;
}

static inline void inputserver_setTid(inputserver_t this, pthread_t tid) {
  this->tid = tid;
}

static inline void inputserver_setMsgcnt(inputserver_t this, int c) {
  this->msgcnt = c;
}

static inline void inputserver_incMsgcnt(inputserver_t this, int c) {
  this->msgcnt++;
}

static inline void inputserver_setNumconn(inputserver_t this, int n) {
  this->numconn = n;
}

static inline void inputserver_incNumconn(inputserver_t this) {
  this->numconn++;
}

static inline void inputserver_setCpumask(inputserver_t this, cpu_set_t cpumask)
{
  this->cpumask = cpumask;
}

static void msgbuf_reset(inputserver_msgbuffer_t mb)
{
  mb->hdr.raw = 0;
  mb->n       = 0;
  mb->fsrv    = NULL;
  mb->qe      = NULL;
}


static void msgbuf_dump(inputserver_msgbuffer_t mb)
{
  fprintf(stderr, "%p:hdr.raw:%lx (funcid:%x datalen:%d) n:%d fsrv:%p qe:%p\n",
	  mb, mb->hdr.raw, mb->hdr.fields.funcid, mb->hdr.fields.datalen,
	  mb->n, mb->fsrv, mb->qe);
}

static inputserver_connection_t
inputserver_connection_new(struct sockaddr_storage addr, socklen_t addrlen,
			  int fd, inputserver_t ss)
{
  	inputserver_connection_t co =
	  malloc(sizeof(struct inputserver_connection));
	co->addr         = addr;
	co->addrlen      = addrlen;
	co->fd           = fd;
	co->msgcnt       = 0;
	msgbuf_reset(&co->mbuf);
	co->ss           = ss;
	return co;
}

static void
inputserver_connection_dump(inputserver_t this, struct inputserver_connection *c) 
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

static void inputserver_cleanupConnection(inputserver_t this,
					 inputserver_connection_t co)
{
  struct epoll_event dummyev;
  int epollfd = inputserver_getEpollfd(this);
  SSVLP(1,"co:%p \n\t", co);
  inputserver_connection_dump(this, co);
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
inputserver_findFuncServerAndQueueEntry(inputserver_t this,
				       union ssbench_msghdr *h,
				       funcserver_t *fsrv,
				       queue_entry_t *qe)
{
  funcserver_t tmpfsrv;
  ASSERT(h);
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


static void inputserver_serveConnection(inputserver_t this,
				       inputserver_connection_t co)
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
  struct inputserver_msgbuffer *comb = &co->mbuf;
  const int msghdrlen = sizeof(union ssbench_msghdr);
  int n = comb->n; 
  queue_entry_t qe;
  
  SSVLP(1,"co:%p mb.n=%d\n",co,comb->n);
  if ( n < msghdrlen) {
    ASSERT(n<=msghdrlen);
    n += net_nonblocking_readn(cofd, &comb->hdr.buf[n], msghdrlen-n);
    comb->n = n;
    if (n<msghdrlen) return; // have not received full msg hdr yet
    // We have a whole message header. Switch over to buffering
    // the data of the message to the correct operator queue
    SSVLP(1,"msghdr: opid:%x(%02hhx %02hhx %02hhx %02hhx) "
	 "datalen:%u (%02hhx %02hhx %02hhx %02hhx)\n",
	 comb->hdr.fields.funcid, comb->hdr.buf[0],
	 comb->hdr.buf[1], comb->hdr.buf[2], comb->hdr.buf[3],
	 comb->hdr.fields.datalen, comb->hdr.buf[4],
	 comb->hdr.buf[5], comb->hdr.buf[6], comb->hdr.buf[7]);
    QueueEntryFindRC_t  qerc;
    funcserver_t fsrv;
    qerc=inputserver_findFuncServerAndQueueEntry(this, &comb->hdr, &fsrv, &qe);
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
  ASSERT(n>=msghdrlen);
  n -= msghdrlen;      // n = amount of message data read so far
  const int len = comb->hdr.fields.datalen;
  ASSERT(n<=len);
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
    ASSERT(fsrv);
    qe->len = comb->n - msghdrlen;
    ASSERT(qe->len == len);
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
static void * inputserver_thread_loop(void * arg)
{
  inputserver_t this = arg;
  // dummy connection object to use as
  // an epoll handle could have used &listenfd
  // but decided this looks better ;-)
  struct {
    int fd;
  } listenConnection = {
    .fd =  inputserver_getListenFd(this)
  };
  int id = inputserver_getId(this);
  int epollfd = inputserver_getEpollfd(this);
  pthread_t tid = pthread_self();
  char *name = inputserver_getName(this);
  struct epoll_event ev, events[MAX_EVENTS];
  
  inputserver_setTid(this, tid);
  snprintf(name,inputserver_sizeofName(this),"is%d",
	   inputserver_getPort(this));
  pthread_setname_np(tid, name);
  
  if (verbose(1)) {
    cpu_set_t cpumask;
    int rc = pthread_getaffinity_np(tid, sizeof(cpumask), &cpumask);
    ASSERT(rc==0);
    SSVLP(1,"%s", "cpuaffinity:");
    cpusetDump(stderr, &cpumask);
  }

  SSVLP(1,"listenConnection:%p:listenFd:%d:epollfd:%d\n",
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
      SSVLP(1,"activeConnection:%p\n", activeConnection);
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
        SSVLP(1,"new connection:%d\n\t", connfd);
	// epoll man page recommends non-blocking io for edgetriggered use 
	net_setnonblocking(connfd);
	// create a new connection object for this connection
	inputserver_connection_t co = inputserver_connection_new(addr, addrlen,
							       connfd, this);
	inputserver_incNumconn(this);
	inputserver_connection_dump(this, co);

	// setup event structure
	ev.events   = EPOLLIN | EPOLLET | EPOLLHUP | EPOLLRDHUP | EPOLLERR;
	ev.data.ptr = co;
	
	if (epoll_ctl(epollfd, EPOLL_CTL_ADD, connfd, &ev) == -1) {
	  perror("epoll_ctl: failed to add connfd");
	  exit(EXIT_FAILURE);
	}
      } else {
	inputserver_connection_t co = activeConnection;
	uint32_t evnts = events[n].events;
	SSVLP(1,"activity on connection:%p: events0x%x fd:%d\n",
	     co, evnts, co->fd);
	// process each of the events that have occurred on this connection
	if (evnts & EPOLLIN) {
	  SSVLP(1,"co:%p fd:%d got EPOLLIN(%x) evnts:%x\n",
	       co, co->fd, EPOLLIN, evnts);
	  inputserver_serveConnection(this, co);
	  evnts = evnts & ~EPOLLIN;
	}
	if (evnts & EPOLLHUP) {
	  SSVLP(1,"co:%p fd:%d got EPOLLHUP(%x) evnts:%x\n",
	       co, co->fd, EPOLLHUP, evnts);
	  evnts = evnts & ~EPOLLHUP;
	  NYI;
	}
	if (evnts & EPOLLRDHUP) {
	  SSVLP(1,"connection lost: co:%p fd:%d got EPOLLRDHUP(%x) evnts:%x\n",
	       co, co->fd, EPOLLRDHUP, evnts);
	  evnts = evnts & ~EPOLLRDHUP;
	  inputserver_cleanupConnection(this, co);
	}
	if (evnts & EPOLLERR) {
	  SSVLP(1,"co:%p fd:%d got EPOLLERR(%x) evnts:%x",
	       co, co->fd, EPOLLERR, evnts);
	  evnts = evnts & ~EPOLLERR;
	  NYI;
	}
	if (evnts != 0) {
	  SSVLP(1,"co:%p fd:%d unknown events evnts:%x",
	       co, co->fd, evnts);
	  ASSERT(evnts==0);
	}
      }
    }		   
  }
}

static void
inputserver_init(inputserver_t this, int port, int id, cpu_set_t cpumask)
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
  net_setnonblocking(fd);
  
  inputserver_setListenFd(this, fd);
  inputserver_setPort(this, port);
  inputserver_setId(this, id);
  inputserver_setMsgcnt(this, 0);
  inputserver_setNumconn(this, 0); 
  inputserver_setCpumask(this, cpumask);
  
  if (net_listen(fd) < 0) {
    VLPRINT(0, "Error: net_listen: %d:%d", fd, port);
    return;
  }
  {
    int epollfd = epoll_create1(0);
    ASSERT(epollfd != -1);
    inputserver_setEpollfd(this,epollfd);
  }
  
  if (verbose(2)) inputserver_dump(this, stderr);

}

extern void
inputserver_start(inputserver_t this, bool async)
{
  pthread_t tid;
  int rc;
  cpu_set_t cpumask = inputserver_getCpumask(this);
  
  if (async) {
    pthread_attr_t attr;
    pthread_attr_t *attrp;
    
    attrp = &attr;
    rc = pthread_attr_init(attrp);
    ASSERT(rc==0);
    rc = pthread_attr_setaffinity_np(attrp, sizeof(cpumask), &cpumask);
    ASSERT(rc==0);
    // start thread loop
    rc = pthread_create(&tid, attrp, &inputserver_thread_loop, this);
    ASSERT(rc==0);
    rc = pthread_attr_destroy(attrp);
    ASSERT(rc==0);
  } else {
    // set and confirm affinity
    tid = pthread_self();
    rc = pthread_setaffinity_np(tid, sizeof(cpumask), &cpumask);
    ASSERT(rc==0);
    inputserver_thread_loop(this);
  }
}

inputserver_t
inputserver_new(int port, int id, cpu_set_t cpumask) {
  inputserver_t this;
  
  this = malloc(sizeof(struct inputserver));
  ASSERT(this);
  inputserver_init(this, port, id, cpumask);  
  return this;
}

void
inputserver_dump(inputserver_t this, FILE *file)
{
  fprintf(file, "inputserver:%p id:%d tid:%ld listenFd:%d port:%d\n", this,
	  inputserver_getId(this),
	  inputserver_getTid(this),
	  inputserver_getListenFd(this),
	  inputserver_getPort(this));
}

extern void
inputserver_destroy(inputserver_t this)
{
  SSVLP(1,"%s", "called\n");
}
