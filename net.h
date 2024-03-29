#ifndef __DA_GAME_NET_H__
#define __DA_GAME_NET_H__
/******************************************************************************
* Copyright (C) 2011 by Jonathan Appavoo, Boston University
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
* THE SOFTWARE.
*****************************************************************************/

typedef int FDType;
typedef int PortType;

#define LISTENQ 1024

extern int     net_setup_listen_socket(FDType *fd, PortType *port);
extern int     net_setup_connection(FDType *fd, char *host, PortType port);
extern int     net_listen(FDType fd);
extern int     net_accept(FDType fd, void *addr, void *addrlen);

extern ssize_t net_writen(FDType fd, const void *vptr, size_t n);
extern ssize_t net_readn(FDType fd, void *vptr, size_t n);
extern void    net_setnonblocking(int fd);
extern ssize_t net_nonblocking_readn(FDType fd, void *vptr, size_t n);
extern ssize_t net_nonblocking_writenn(FDType fd, void *vptr, size_t n);

#endif
