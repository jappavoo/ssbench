#define _GNU_SOURCE
#include "ssbench.h"
#include "hexdump.h"
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <errno.h>

//012345678901234567890123456789012345678901234567890123456789012345678901234567
#define IVLP(VL,fmt,...)				  \
  VLPRINT(VL,"%p:%d:%lx:" fmt, this, input_getId(this),	\
	 input_getTid(this), __VA_ARGS__)

static inline void input_setPort(input_t this, int p)
{
  this->port = p;
}

static inline
void input_setListenFd(input_t this, int fd)
{
  this->listenFd = fd;
}

static inline void
input_setId(input_t this, inputid_t id)
{
  this->id = id;
}

static inline void
input_setEpollfd(input_t this, int fd)
{
  this->epollfd = fd;
}

static inline void
input_setTid(input_t this, pthread_t tid)
{
  this->tid = tid;
}

static inline void
input_setMsgcnt(input_t this, int c)
{
  this->msgcnt = c;
}

static inline
void input_incMsgcnt(input_t this, int c)
{
  this->msgcnt++;
}

static inline void
input_setNumconn(input_t this, int n)
{
  this->numconn = n;
}

static inline void
input_incNumconn(input_t this) {
  this->numconn++;
}

static inline void
input_setCpumask(input_t this, cpu_set_t cpumask)
{
  this->cpumask = cpumask;
}

static void
msgbuf_reset(input_msgbuffer_t mb)
{
  mb->hdr.raw[0] = 0;
  _Static_assert(sizeof(msghdr_bytes_t)==(sizeof(uint64_t)*1),
		 "missmatch msg raw size");
  mb->n       = 0;
  mb->worker  = NULL;
  mb->qe      = NULL;
}


static void
msgbuf_dump(input_msgbuffer_t mb)
{
  fprintf(stderr, "%p:hdr.raw:%lx (workerid:%x datalen:%d) n:%d fsrv:%p qe:%p\n",
	  mb, mb->hdr.raw[0], mb->hdr.fields.wq.workerid,
	  mb->hdr.fields.datalen,
	  mb->n, mb->worker, mb->qe);
}

static input_connection_t
input_connection_new(struct sockaddr_storage addr, socklen_t addrlen,
			  int fd, input_t input)
{
  	input_connection_t co = malloc(sizeof(struct input_connection));
	co->addr              = addr;
	co->addrlen           = addrlen;
	co->fd                = fd;
	co->msgcnt            = 0;
	msgbuf_reset(&co->mbuf);
	co->input             = input;
	return co;
}

static void
input_connection_dump(input_t this, struct input_connection *c) 
{
  int connfd = c->fd;
  struct sockaddr *addr = (struct sockaddr *)&(c->addr);
  socklen_t addrlen = c->addrlen;
  int msgcnt = c->msgcnt;
  void *input = c->input;
    
  if (verbose(1)) {
    fprintf(stderr,"%p:input:%p fd:%d msgcnt:%d sa_fam:%d addrlen:%d addr:",
	    c,input, connfd, msgcnt, addr->sa_family, addrlen);
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

static void
input_cleanupConnection(input_t this, input_connection_t co)
{
  struct epoll_event dummyev;
  int epollfd = input_getEpollfd(this);
  IVLP(1,"co:%p \n\t", co);
  input_connection_dump(this, co);
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
input_findWorkerAndQueueEntry(input_t this, union ssbench_msghdr *h,
			      worker_t *w, queue_entry_t *qe)
{
  worker_t tmpw;
  ASSERT(h);
  workerid_t wid = h->fields.wq.workerid;
  int intwid = wid;
  
  HASH_FIND_INT(Args.workers.hashtable, &intwid, tmpw);

  if (tmpw == NULL) {
    // no worker found for this id
    VLPRINT(2, "NO worker found for wid=%d in message.\n",wid);
    *w = NULL;
    *qe   = NULL;
    return Q_NONE;
  }
  *w = tmpw;
  return  worker_getQueueEntry(tmpw, h, qe);
}


static void
input_serveConnection(input_t this, input_connection_t co)
{
  // We got data on the fd our job is to buffer a message to a work
  // and then let the worker processes it.  Given that a socket is a serial
  // stream of bytes we will assume that messages are contigous and that
  // as single message is destined to a single worker.  A future extention
  // would be to support broadcast and multicast but that is not something
  // we are supporting now.

  // Each connection object contains the state of the current message begin
  // buffered.  Once a message is released to a work the connections message
  // buffer needs to be reset.
  int cofd                     = co->fd;
  struct input_msgbuffer *comb = &co->mbuf;
  const int msghdrlen          = sizeof(union ssbench_msghdr);
  int n                        = comb->n; 
  queue_entry_t qe;
  
  IVLP(1,"co:%p mb.n=%d\n",co,comb->n);
  if ( n < msghdrlen) {
    ASSERT(n<=msghdrlen);
    n += net_nonblocking_readn(cofd, &comb->hdr.buf[n], msghdrlen-n);
    comb->n = n;
    if (n<msghdrlen) return; // have not received full msg hdr yet
    // We have a whole message header. Switch over to buffering
    // the data of the message to the correct operator queue
    IVLP(1,"msghdr: wid:%x(%02hhx %02hhx %02hhx %02hhx) "
	 "datalen:%u (%02hhx %02hhx %02hhx %02hhx)\n",
	 comb->hdr.fields.wq.workerid, comb->hdr.buf[0],
	 comb->hdr.buf[1], comb->hdr.buf[2], comb->hdr.buf[3],
	 comb->hdr.fields.datalen, comb->hdr.buf[4],
	 comb->hdr.buf[5], comb->hdr.buf[6], comb->hdr.buf[7]);
    QueueEntryFindRC_t  qerc;
    worker_t w;
    qerc=input_findWorkerAndQueueEntry(this, &comb->hdr, &w, &qe);
    if (qerc == Q_NONE) {
      VLPRINT(2, "input:%p Q_NONE could not find an worker?\n", this); 
      comb->qe = NULL;
    } else if (qerc == Q_MSG2BIG) {
      EPRINT("input:%p worker:%p QMSG2BIG\n", this, w);
      comb->qe = NULL;
    } else if (qerc == Q_FULL) {
      // add code to record and signal back pressure
      EPRINT("input:%p w:%p wid:%04hx Q_FULL no space to place message drain\n",
	     this, w, worker_getId(w));
      comb->qe = NULL;
    } else {
      comb->qe      = qe;
      comb->worker  = w;
    }
  }
  // buffer data of message to the queue element
  qe = comb->qe;
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
      hexdump(stderr, (uint8_t *)tmpbuf, tmpn); 
    }
    n += tmpn;
  } else {
    // sizes should all have been validated already
    uint8_t *buf = qe->data;
    n += net_nonblocking_readn(cofd, buf, len-n);
  }
  comb->n = msghdrlen + n; // record how much data we have read in total
  if (n<len) return; // have not recieved all the message data yet
    
  // message has been received then release to worker
  if (qe != NULL) {
    worker_t worker = comb->worker;
    ASSERT(worker);
    qe->len = comb->n - msghdrlen;
    ASSERT(qe->len == len);
    if (verbose(2)) {
      VLPRINT(2, "Got msg for worker:%p id:%04hx\n",
	      worker, worker_getId(worker));
      hexdump(stderr, qe->data, qe->len);
    }
    worker_putBackQueueEntry(worker, &qe);
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
static void * input_thread_loop(void * arg)
{
  input_t this = arg;
  // dummy connection object to use as
  // an epoll handle could have used &listenfd
  // but decided this looks better ;-)
  struct {
    int fd;
  } listenConnection = {
    .fd =  input_getListenFd(this)
  };
  int epollfd = input_getEpollfd(this);
  pthread_t tid = pthread_self();
  char *name = input_getName(this);
  struct epoll_event ev, events[MAX_EVENTS];
  
  input_setTid(this, tid);
  snprintf(name,input_sizeofName(this),"is%d",
	   input_getPort(this));
  pthread_setname_np(tid, name);
  
  if (verbose(1)) {
    cpu_set_t cpumask;
    int rc = pthread_getaffinity_np(tid, sizeof(cpumask), &cpumask);
    ASSERT(rc==0);
    IVLP(1,"%s", "cpuaffinity:");
    cpusetDump(stderr, &cpumask);
  }

  IVLP(1,"listenConnection:%p:listenFd:%d:epollfd:%d\n",
       &listenConnection,listenConnection.fd, epollfd);

  ev.events = EPOLLIN;
  ev.data.ptr = &listenConnection;

  if (epoll_ctl(epollfd, EPOLL_CTL_ADD, listenConnection.fd, &ev) == -1) {
    perror("epoll_ctl: listenfd");
    exit(EXIT_FAILURE);
  }

  pthread_barrier_wait(&(Args.inputs.barrier));
  
  for (;;) {
    int nfds = epoll_wait(epollfd, events, MAX_EVENTS, -1);
    if (nfds == -1) {
      perror("epoll_wait");
      exit(EXIT_FAILURE);
    }
    
    for (int n = 0; n < nfds; ++n) {
      void * activeConnection = events[n].data.ptr;
      IVLP(1,"activeConnection:%p\n", activeConnection);
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
        IVLP(1,"new connection:%d\n\t", connfd);
	// epoll man page recommends non-blocking io for edgetriggered use 
	net_setnonblocking(connfd);
	// create a new connection object for this connection
	input_connection_t co = input_connection_new(addr, addrlen,
							       connfd, this);
	input_incNumconn(this);
	input_connection_dump(this, co);

	// setup event structure
	ev.events   = EPOLLIN | EPOLLET | EPOLLHUP | EPOLLRDHUP | EPOLLERR;
	ev.data.ptr = co;
	
	if (epoll_ctl(epollfd, EPOLL_CTL_ADD, connfd, &ev) == -1) {
	  perror("epoll_ctl: failed to add connfd");
	  exit(EXIT_FAILURE);
	}
      } else {
	input_connection_t co = activeConnection;
	uint32_t evnts = events[n].events;
	IVLP(1,"activity on connection:%p: events0x%x fd:%d\n",
	     co, evnts, co->fd);
	// process each of the events that have occurred on this connection
	if (evnts & EPOLLIN) {
	  IVLP(1,"co:%p fd:%d got EPOLLIN(%x) evnts:%x\n",
	       co, co->fd, EPOLLIN, evnts);
	  input_serveConnection(this, co);
	  evnts = evnts & ~EPOLLIN;
	}
	if (evnts & EPOLLHUP) {
	  IVLP(1,"co:%p fd:%d got EPOLLHUP(%x) evnts:%x\n",
	       co, co->fd, EPOLLHUP, evnts);
	  evnts = evnts & ~EPOLLHUP;
	  NYI;
	}
	if (evnts & EPOLLRDHUP) {
	  IVLP(1,"connection lost: co:%p fd:%d got EPOLLRDHUP(%x) evnts:%x\n",
	       co, co->fd, EPOLLRDHUP, evnts);
	  evnts = evnts & ~EPOLLRDHUP;
	  input_cleanupConnection(this, co);
	}
	if (evnts & EPOLLERR) {
	  IVLP(1,"co:%p fd:%d got EPOLLERR(%x) evnts:%x",
	       co, co->fd, EPOLLERR, evnts);
	  evnts = evnts & ~EPOLLERR;
	  NYI;
	}
	if (evnts != 0) {
	  IVLP(1,"co:%p fd:%d unknown events evnts:%x",
	       co, co->fd, evnts);
	  ASSERT(evnts==0);
	}
      }
    }		   
  }
}

static void
input_init(input_t this, int port, inputid_t id, cpu_set_t cpumask)
{
  int rc;
  int fd;
  
  rc=net_setup_listen_socket(&fd, &port);

  if (rc==0) { 
    EPRINT("ERROR: net_setup_listen_socket: fd=%d port=%d\n", fd, port);
    return;
  }

  // we are using epoll so we set the listen fd to non-blocking mode
  net_setnonblocking(fd);
  
  input_setListenFd(this, fd);
  input_setPort(this, port);
  input_setId(this, id);
  input_setMsgcnt(this, 0);
  input_setNumconn(this, 0); 
  input_setCpumask(this, cpumask);
  
  if (net_listen(fd) < 0) {
    EPRINT("ERROR: net_listen: %d:%d", fd, port);
    return;
  }
  {
    int epollfd = epoll_create1(0);
    ASSERT(epollfd != -1);
    input_setEpollfd(this,epollfd);
  }
  
  if (verbose(2)) input_dump(this, stderr);

}

extern void
input_start(input_t this, bool async)
{
  pthread_t tid;
  int rc;
  cpu_set_t cpumask = input_getCpumask(this);
  
  if (async) {
    pthread_attr_t attr;
    pthread_attr_t *attrp;
    
    attrp = &attr;
    rc = pthread_attr_init(attrp);
    ASSERT(rc==0);
    rc = pthread_attr_setaffinity_np(attrp, sizeof(cpumask), &cpumask);
    ASSERT(rc==0);
    // start thread loop
    rc = pthread_create(&tid, attrp, &input_thread_loop, this);
    ASSERT(rc==0);
    rc = pthread_attr_destroy(attrp);
    ASSERT(rc==0);
  } else {
    // set and confirm affinity
    tid = pthread_self();
    rc = pthread_setaffinity_np(tid, sizeof(cpumask), &cpumask);
    ASSERT(rc==0);
    input_thread_loop(this);
  }
}

input_t
input_new(int port, inputid_t id, cpu_set_t cpumask) {
  input_t this;
  
  this = malloc(sizeof(struct input));
  ASSERT(this);
  input_init(this, port, id, cpumask);  
  return this;
}

void
input_dump(input_t this, FILE *file)
{
  fprintf(file, "input:%p id:%hd tid:%ld listenFd:%d port:%d\n", this,
	  input_getId(this),
	  input_getTid(this),
	  input_getListenFd(this),
	  input_getPort(this));
}

extern void
input_destroy(input_t this)
{
  IVLP(1,"%s", "called\n");
}
