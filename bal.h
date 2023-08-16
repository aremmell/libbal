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

# include "balinternal.h"
# include "balerrors.h"
# include "baltypes.h"
# include "balhelpers.h"

/******************************************************************************\
 *                             Exported Functions                             *
\******************************************************************************/

# if defined(__cplusplus)
extern "C" {
# endif

# define bal_init _bal_init
# define bal_cleanup _bal_cleanup

# define bal_asyncselect(s, proc, mask) _bal_asyncselect(s, proc, mask)

int bal_autosocket(bal_socket** s, int addr_fam, int proto, const char* host, const char* port);
int bal_sock_create(bal_socket** s, int addr_fam, int type, int proto);
# define bal_sock_destroy(s) _bal_sock_destroy(s)
int bal_close(bal_socket** s, bool destroy);
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
int bal_accept(const bal_socket* s, bal_socket** res, bal_sockaddr* resaddr);

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

bool bal_isreadable(const bal_socket* s);
bool bal_iswritable(const bal_socket* s);
bool bal_islistening(const bal_socket* s);

static inline
void bal_addtomask(bal_socket* s, uint32_t bits)
{
    if (_bal_validsock(s))
        bal_setbitshigh(&s->state.mask, bits);
}

static inline
void bal_remfrommask(bal_socket* s, uint32_t bits)
{
    if (_bal_validsock(s))
        bal_setbitslow(&s->state.mask, bits);
}

static inline
bool bal_isbitinmask(bal_socket* s, uint32_t bit)
{
    return _bal_validsock(s) ? bal_isbitset(s->state.mask, bit) : false;
}

int bal_setiomode(const bal_socket* s, bool async);
size_t bal_recvqueuesize(const bal_socket* s);

int bal_getlasterror(bal_error* err);

int bal_resolvehost(const char* host, bal_addrlist* out);
int bal_getremotehostaddr(const bal_socket* s, bal_sockaddr* out);
int bal_getremotehoststrings(const bal_socket* s, int dns, bal_addrstrings* out);
int bal_getlocalhostaddr(const bal_socket* s, bal_sockaddr* out);
int bal_getlocalhoststrings(const bal_socket* s, int dns, bal_addrstrings* out);
int bal_getaddrstrings(const bal_sockaddr* in, bool dns, bal_addrstrings* out);

int bal_resetaddrlist(bal_addrlist* al);
const bal_sockaddr* bal_enumaddrlist(bal_addrlist* al);
int bal_freeaddrlist(bal_addrlist* al);

void bal_thread_yield(void);
void bal_sleep_msec(uint32_t msec);

# if defined(__cplusplus)
}
# endif

#endif /* !_BAL_H_INCLUDED */
