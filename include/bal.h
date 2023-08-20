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

# include "bal/internal.h"
# include "bal/errors.h"
# include "bal/types.h"
# include "bal/helpers.h"
# include "bal/version.h"

/******************************************************************************\
 *                             Exported Functions                             *
\******************************************************************************/

# if defined(__cplusplus)
extern "C" {
# endif

bool bal_init(void);
bool bal_cleanup(void);
#pragma message("TODO: continue renaming")
bool bal_asyncpoll(bal_socket* s, bal_async_cb proc, uint32_t mask);

bool bal_autosocket(bal_socket** s, int addr_fam, int proto, const char* host,
    const char* srv);
bool bal_sock_create(bal_socket** s, int addr_fam, int type, int proto);
void bal_sock_destroy(bal_socket** s);
bool bal_close(bal_socket** s, bool destroy);
bool bal_shutdown(bal_socket* s, int how);

bool bal_connect(bal_socket* s, const char* host, const char* port);
bool bal_connect_addrlist(bal_socket* s, bal_addrlist* al);

int bal_send(const bal_socket* s, const void* data, bal_iolen len, int flags);
int bal_recv(const bal_socket* s, void* data, bal_iolen len, int flags);

int bal_sendto(const bal_socket* s, const char* host, const char* port, const void* data,
    bal_iolen len, int flags);
int bal_sendtoaddr(const bal_socket* s, const bal_sockaddr* sa, const void* data,
    bal_iolen len, int flags);

int bal_recvfrom(const bal_socket* s, void* data, bal_iolen len, int flags, bal_sockaddr* res);

bool bal_bind(const bal_socket* s, const char* addr, const char* srv);
bool bal_bindall(const bal_socket* s, const char* srv);

bool bal_listen(bal_socket* s, int backlog);
bool bal_accept(const bal_socket* s, bal_socket** res, bal_sockaddr* resaddr);

bool bal_getoption(const bal_socket* s, int level, int name, void* optval, socklen_t len);
bool bal_setoption(const bal_socket* s, int level, int name, const void* optval, socklen_t len);

bool bal_setbroadcast(const bal_socket* s, int value);
bool bal_getbroadcast(const bal_socket* s, int* value);

bool bal_setdebug(const bal_socket* s, int value);
bool bal_getdebug(const bal_socket* s, int* value);

bool bal_setlinger(const bal_socket* s, bal_linger sec);
bool bal_getlinger(const bal_socket* s, bal_linger* sec);

bool bal_setkeepalive(const bal_socket* s, int value);
bool bal_getkeepalive(const bal_socket* s, int* value);

bool bal_setoobinline(const bal_socket* s, int value);
bool bal_getoobinline(const bal_socket* s, int* value);

bool bal_setreuseaddr(const bal_socket* s, int value);
bool bal_getreuseaddr(const bal_socket* s, int* value);

bool bal_setsendbufsize(const bal_socket* s, int size);
bool bal_getsendbufsize(const bal_socket* s, int* size);

bool bal_setrecvbufsize(const bal_socket* s, int size);
bool bal_getrecvbufsize(const bal_socket* s, int* size);

bool bal_setsendtimeout(const bal_socket* s, long sec, long usec);
bool bal_getsendtimeout(const bal_socket* s, long* sec, long* usec);

bool bal_set_recv_timeout(const bal_socket* s, long sec, long usec);
bool bal_get_recv_timeout(const bal_socket* s, long* sec, long* usec);

int bal_geterror(const bal_socket* s);

bool bal_isreadable(const bal_socket* s);
bool bal_iswritable(const bal_socket* s);
bool bal_islistening(const bal_socket* s);

static inline
void bal_addtomask(bal_socket* s, uint32_t bits)
{
    if (_bal_validptr(s))
        bal_setbitshigh(&s->state.mask, bits);
}

static inline
void bal_remfrommask(bal_socket* s, uint32_t bits)
{
    if (_bal_validptr(s))
        bal_setbitslow(&s->state.mask, bits);
}

static inline
bool bal_bitsinmask(const bal_socket* s, uint32_t bits)
{
    return _bal_validptr(s) ? bal_isbitset(s->state.mask, bits) : false;
}

bool bal_set_io_mode(const bal_socket* s, bool async);
size_t bal_get_recvqueue_size(const bal_socket* s);

int bal_getlasterror(const bal_socket* s, bal_error* err);

bool bal_resolve_host(const char* host, bal_addrlist* out);
bool bal_get_peer_addr(const bal_socket* s, bal_sockaddr* out);
bool bal_get_peer_host_strings(const bal_socket* s, int dns, bal_addrstrings* out);
bool bal_get_localhost_addr(const bal_socket* s, bal_sockaddr* out);
bool bal_get_localhost_strings(const bal_socket* s, int dns, bal_addrstrings* out);
bool bal_get_addrstrings(const bal_sockaddr* in, bool dns, bal_addrstrings* out);

bool bal_reset_addrlist(bal_addrlist* al);
const bal_sockaddr* bal_enum_addrlist(bal_addrlist* al);
bool bal_free_addrlist(bal_addrlist* al);

void bal_thread_yield(void);
void bal_sleep_msec(uint32_t msec);

# if defined(__cplusplus)
}
# endif

#endif /* !_BAL_H_INCLUDED */
