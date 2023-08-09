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

#include "balplatform.h"
#include "baltypes.h"


/*─────────────────────────────────────────────────────────────────────────────╮
│                            Exported Functions                                │
╰─────────────────────────────────────────────────────────────────────────────*/


# if defined(__cplusplus)
extern "C" {
# endif

int bal_initialize(void);
int bal_finalize(void);

int bal_asyncselect(const bal_socket* s, bal_async_callback proc, uint32_t mask);

int bal_autosocket(bal_socket* s, int af, int pt, const char* host, const char* port);
int bal_sock_create(bal_socket* s, int af, int pt, int st);
void bal_reset(bal_socket* s);
int bal_close(bal_socket* s);
int bal_shutdown(bal_socket* s, int how);

int bal_connect(const bal_socket* s, const char* host, const char* port);
int bal_connectaddrlist(bal_socket* s, bal_addrlist* al);

int bal_send(const bal_socket* s, const void* data, size_t len, int flags);
int bal_recv(const bal_socket* s, void* data, size_t len, int flags);

int bal_sendto(const bal_socket* s, const char* host, const char* port,
    const void* data, size_t len, int flags);
int bal_sendtoaddr(const bal_socket* s, const bal_sockaddr* sa, const void* data,
    size_t len, int flags);

int bal_recvfrom(const bal_socket* s, void* data, size_t len, int flags,
    bal_sockaddr* res);

int bal_bind(const bal_socket* s, const char* addr, const char* port);

int bal_listen(bal_socket* s, int backlog);
int bal_accept(const bal_socket* s, bal_socket* res, bal_sockaddr* resaddr);

int bal_getoption(const bal_socket* s, int level, int name, void* optval,
    socklen_t len);
int bal_setoption(const bal_socket* s, int level, int name, const void* optval,
    socklen_t len);

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

int bal_setiomode(const bal_socket* s, bool async);
size_t bal_recvqueuesize(const bal_socket* s);

int bal_lastliberror(bal_error* err);
int bal_lastsockerror(const bal_socket* s, bal_error* err);

int bal_resolvehost(const char* host, bal_addrlist* out);
int bal_getremotehostaddr(const bal_socket* s, bal_sockaddr* out);
int bal_getremotehoststrings(const bal_socket* s, int dns, bal_addrstrings* out);
int bal_getlocalhostaddr(const bal_socket* s, bal_sockaddr* out);
int bal_getlocalhoststrings(const bal_socket* s, int dns, bal_addrstrings* out);

int bal_resetaddrlist(bal_addrlist* al);
const bal_sockaddr* bal_enumaddrlist(bal_addrlist* al);
int bal_freeaddrlist(bal_addrlist* al);
int bal_getaddrstrings(const bal_sockaddr* in, bool dns, bal_addrstrings* out);

static inline
bool bal_isbitset(uint32_t bitmask, uint32_t bit)
{
    return (bitmask & bit) == bit;
}

/*─────────────────────────────────────────────────────────────────────────────╮
│                             Internal functions                               │
╰─────────────────────────────────────────────────────────────────────────────*/


int _bal_bindany(const bal_socket* s, unsigned short port);
int _bal_getaddrinfo(int f, int af, int st, const char* host, const char* port,
    bal_addrinfo* res);
int _bal_getnameinfo(int f, const bal_sockaddr* in, char* host, char* port);

const struct addrinfo* _bal_enumaddrinfo(bal_addrinfo* ai);
int _bal_aitoal(bal_addrinfo* in, bal_addrlist* out);

int _bal_getlasterror(const bal_socket* s, bal_error* err);
void _bal_setlasterror(int err);

int _bal_retstr(char* out, const char* in);

int _bal_haspendingconnect(const bal_socket* s);
int _bal_isclosedcircuit(const bal_socket* s);

BALTHREAD _bal_eventthread(void* p);
int _bal_initasyncselect(bal_thread* t, bal_mutex* m, bal_eventthread_data * td);
void _bal_dispatchevents(fd_set* set, bal_eventthread_data * td, int type);

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
int _bal_mutex_destroy(bal_mutex* m);

bool _bal_once(bal_once* once, bal_once_fn func);

# if defined(__WIN__)
BOOL CALLBACK _bal_static_once_init(PINIT_ONCE ponce, PVOID param, PVOID* ctx);
# else
void _bal_static_once_init(void);
# endif

# if defined(__cplusplus)
}
# endif

#endif /* !_BAL_H_INCLUDED */
