/*
 * bal.h
 *
 * Author:    Ryan M. Lederman <lederman@gmail.com>
 * Copyright: Copyright (c) 2004-2023
 * Version:   0.2.0
 * License:   The MIT License (MIT)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#ifndef _BAL_H_INCLUDED
# define _BAL_H_INCLUDED

# include <stdio.h>
# include <stdarg.h>
# include <stdbool.h>
# include <stdint.h>

# if !defined(_WIN32)
#  include <sys/types.h>
#  include <sys/socket.h>
#  include <sys/select.h>
#  include <sys/time.h>
#  include <sys/ioctl.h>
#  include <netinet/in.h>
#  include <arpa/inet.h>
#  include <netdb.h>
#  include <unistd.h>
#  include <errno.h>
#  include <stdlib.h>
#  include <string.h>
#  include <pthread.h>
#  include <sched.h>

#  if defined(__sun)
#   include <sys/filio.h>
#   include <stropts.h>
#  endif

typedef int bal_socket;
typedef pthread_mutex_t bal_mutex;
typedef pthread_t bal_thread;

# else /* __WIN__ */
#  define __WIN__
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  include <process.h>
#  include <time.h>
#  include <tchar.h>

#  define WSOCK_MAJVER 2
#  define WSOCK_MINVER 2

typedef SOCKET bal_descriptor;
typedef HANDLE bal_mutex;
typedef HANDLE bal_thread;
# endif

typedef char bchar, *bstr;
typedef const char* cbstr;

# define BAL_TRUE 0
# define BAL_FALSE -1
# define BAL_ERROR BAL_FALSE
# define BAL_MAXERROR 1024

# define BAL_AS_UNKNWN "<unknown>"
# define BAL_AS_IPV6   "IPv6"
# define BAL_AS_IPV4   "IPv4"

# define BAL_F_PENDCONN 0x00000001UL
# define BAL_F_CLOSELCL 0x00000002UL

# define BAL_BADSOCKET -1

typedef struct sockaddr_storage bal_sockaddr;

/**
 * @struct bal_socket
 * @brief BAL state structure.
 */
typedef struct {
    bal_descriptor sd; /**< Socket descriptor. */
    int af;            /**< Address family (e.g. AF_INET). */
    int st;            /**< Socket type (e.g., SOCK_STREAM). */
    int pf;            /**< Protocol family (e.g., PROTO_TCP). */
    uint32_t _f;       /**< Internally-used state flags. */
} bal_socket;

typedef struct {
    struct addrinfo* _ai;
    struct addrinfo* _p;
} bal_addrinfo;

typedef struct bal_addr {
    bal_sockaddr _sa;
    struct bal_addr* _n;
} bal_addr;

typedef struct {
    bal_addr* _a;
    bal_addr* _p;
} bal_addrlist;

typedef struct {
    bchar host[NI_MAXHOST];
    bchar ip[NI_MAXHOST];
    bstr type;
    bchar port[NI_MAXSERV];
} bal_addrstrings;

typedef struct {
    int code;
    bchar desc[BAL_MAXERROR];
} bal_error;

typedef void (*bal_async_callback)(const bal_socket*, int);

typedef struct bal_selectdata {
    bal_socket* s;
    uint32_t mask;
    bal_async_callback proc;
    struct bal_selectdata* _p;
    struct bal_selectdata* _n;
} bal_selectdata;

typedef struct {
    bal_selectdata* _h;
    bal_selectdata* _t;
    bal_selectdata* _c;
} bal_selectdata_list;

typedef struct {
    bal_selectdata_list* sdl;
    bal_mutex* m;
    int die;
} bal_eventthread_data;

/*─────────────────────────────────────────────────────────────────────────────╮
│                            Exported Functions                                │
╰─────────────────────────────────────────────────────────────────────────────*/

# if defined(__cplusplus)
extern "C" {
# endif

int bal_initialize(void);
int bal_finalize(void);

int bal_asyncselect(const bal_socket* s, bal_async_callback proc, uint32_t mask);

int bal_autosocket(bal_socket* s, int af, int pt, cbstr host, cbstr port);
int bal_sock_create(bal_socket* s, int af, int pt, int st);
int bal_reset(bal_socket* s);
int bal_close(bal_socket* s);
int bal_shutdown(bal_socket* s, int how);

int bal_connect(const bal_socket* s, cbstr host, cbstr port);
int bal_connectaddrlist(bal_socket* s, bal_addrlist* al);

int bal_send(const bal_socket* s, const void* data, size_t len, int flags);
int bal_recv(const bal_socket* s, void* data, size_t len, int flags);

int bal_sendto(const bal_socket* s, cbstr host, cbstr port, const void* data, size_t len, int flags);
int bal_sendtoaddr(const bal_socket* s, const bal_sockaddr* sa, const void* data, size_t len, int flags);

int bal_recvfrom(const bal_socket* s, void* data, size_t len, int flags, bal_sockaddr* res);

int bal_bind(const bal_socket* s, cbstr addr, cbstr port);

int bal_listen(const bal_socket* s, int backlog);
int bal_accept(const bal_socket* s, bal_socket* res, bal_sockaddr* resaddr);

int bal_getoption(const bal_socket* s, int level, int name, void* optval, socklen_t len);
int bal_setoption(const bal_socket* s, int level, int name, const void* optval, socklen_t len);

int bal_setbroadcast(const bal_socket* s, int flag);
int bal_getbroadcast(const bal_socket* s);

int bal_setdebug(const bal_socket* s, int flag);
int bal_getdebug(const bal_socket* s);

int bal_setlinger(const bal_socket* s, int sec);
int bal_getlinger(const bal_socket* s, int* sec);

int bal_setkeepalive(const bal_socket* s, int flag);
int bal_getkeepalive(const bal_socket* s);

int bal_setoobinline(const bal_socket* s, int flag);
int bal_getoobinline(const bal_socket* s);

int bal_setreuseaddr(const bal_socket* s, int flag);
int bal_getreuseaddr(const bal_socket* s);

int bal_setsendbufsize(const bal_socket* s, int size);
int bal_getsendbufsize(const bal_socket* s);

int bal_setrecvbufsize(const bal_socket* s, int size);
int bal_getrecvbufsize(const bal_socket* s);

int bal_setsendtimeout(const bal_socket* s, long sec, long usec);
int bal_getsendtimeout(const bal_socket* s, long* sec, long* usec);

int bal_setrecvtimeout(const bal_socket* s, long sec, long usec);
int bal_getrecvtimeout(const bal_socket* s, long* sec, long* usec);

int bal_geterror(const bal_socket* s);

int bal_islistening(const bal_socket* s);
int bal_isreadable(const bal_socket* s);
int bal_iswritable(const bal_socket* s);

int bal_setiomode(const bal_socket* s, unsigned long flag);
size_t bal_recvqueuesize(const bal_socket* s);

int bal_lastliberror(bal_error* err);
int bal_lastsockerror(const bal_socket* s, bal_error* err);

int bal_resolvehost(cbstr host, bal_addrlist* out);
int bal_getremotehostaddr(const bal_socket* s, bal_sockaddr* out);
int bal_getremotehoststrings(const bal_socket* s, int dns, bal_addrstrings* out);
int bal_getlocalhostaddr(const bal_socket* s, bal_sockaddr* out);
int bal_getlocalhoststrings(const bal_socket* s, int dns, bal_addrstrings* out);

int bal_resetaddrlist(bal_addrlist* al);
const bal_sockaddr* bal_enumaddrlist(bal_addrlist* al);
int bal_freeaddrlist(bal_addrlist* al);
int bal_getaddrstrings(const bal_sockaddr* in, int dns, bal_addrstrings* out);


/*─────────────────────────────────────────────────────────────────────────────╮
│                           Internal definitions                               │
╰─────────────────────────────────────────────────────────────────────────────*/


# define _bal_validstr(str) (str && *str)

# define BAL_SASIZE(sa) \
    ((PF_INET == ((struct sockaddr* )&sa)->sa_family) ? sizeof(struct sockaddr_in) \
                                                      : sizeof(struct sockaddr_in6))

# define BAL_NI_NODNS (NI_NUMERICHOST | NI_NUMERICSERV)
# define BAL_NI_DNS   (NI_NAMEREQD | NI_NUMERICSERV)

# define BAL_E_READ      0x00000001
# define BAL_E_WRITE     0x00000002
# define BAL_E_CONNECT   0x00000004
# define BAL_E_ACCEPT    0x00000008
# define BAL_E_CLOSE     0x00000010
# define BAL_E_CONNFAIL  0x00000020
# define BAL_E_EXCEPTION 0x00000040
# define BAL_E_ALL       0x0000007F

# define BAL_S_DIE       0x0D1E0D1E
# define BAL_S_CONNECT   0x10000000
# define BAL_S_READ      0x00000001
# define BAL_S_WRITE     0x00000002
# define BAL_S_EXCEPT    0x00000003
# define BAL_S_TIME      0x0000C350

int _bal_bindany(const bal_socket* s, unsigned short port);
int _bal_getaddrinfo(int f, int af, int st, cbstr host, cbstr port, bal_addrinfo* res);
int _bal_getnameinfo(int f, const bal_sockaddr* in, bstr host, bstr port);

const struct addrinfo* _bal_enumaddrinfo(bal_addrinfo* ai);
int _bal_aitoal(bal_addrinfo* in, bal_addrlist* out);

int _bal_getlasterror(const bal_socket* s, bal_error* err);
void _bal_setlasterror(int err);

int _bal_retstr(bstr out, const char* in);

int _bal_haspendingconnect(const bal_socket* s);
int _bal_isclosedcircuit(const bal_socket* s);

# if defined(__WIN__)
#  define BALTHREAD unsigned __stdcall
# else
#  define BALTHREAD void*
# endif

BALTHREAD _bal_eventthread(void* p);
int _bal_initasyncselect(bal_thread* t, bal_mutex* m, bal_eventthread_data* td);
void _bal_dispatchevents(fd_set* set, bal_eventthread_data* td, int type);

int _bal_sdl_add(bal_selectdata_list* sdl, const bal_selectdata* d);
int _bal_sdl_rem(bal_selectdata_list* sdl, bal_descriptor sd);
int _bal_sdl_clr(bal_selectdata_list* sdl);
int _bal_sdl_size(bal_selectdata_list* sdl);
int _bal_sdl_enum(bal_selectdata_list* sdl, bal_selectdata** d);
void _bal_sdl_reset(bal_selectdata_list* sdl);
bal_selectdata* _bal_sdl_find(const bal_selectdata_list* sdl, bal_descriptor sd);

int _bal_mutex_init(bal_mutex* m);
int _bal_mutex_lock(bal_mutex* m);
int _bal_mutex_unlock(bal_mutex* m);
int _bal_mutex_free(bal_mutex* m);

# if defined(__cplusplus)
}
# endif
#endif /* !_BAL_H_INCLUDED */
