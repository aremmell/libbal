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

# if defined(__cplusplus)
extern "C" {
# endif

bool bal_init(void);
bool bal_cleanup(void);
bool bal_isinitialized(void);

bool bal_async_poll(bal_socket* s, bal_async_cb proc, uint32_t mask);

bool bal_create(bal_socket** s, int addr_fam, int type, int proto);
bool bal_auto_socket(bal_socket** s, int addr_fam, int proto, const char* host, const char* srv);
void bal_destroy(bal_socket** s);
bool bal_close(bal_socket** s, bool destroy);
bool bal_shutdown(bal_socket* s, int how);

bool bal_connect(bal_socket* s, const char* host, const char* port);
bool bal_connect_addrlist(bal_socket* s, bal_addrlist* al);

ssize_t bal_send(const bal_socket* s, const void* data, bal_iolen len, int flags);
ssize_t bal_recv(const bal_socket* s, void* data, bal_iolen len, int flags);

ssize_t bal_sendto(const bal_socket* s, const char* host, const char* port, const void* data,
    bal_iolen len, int flags);
ssize_t bal_sendto_addr(const bal_socket* s, const bal_sockaddr* sa, const void* data,
    bal_iolen len, int flags);

ssize_t bal_recvfrom(const bal_socket* s, void* data, bal_iolen len, int flags, bal_sockaddr* res);

bool bal_bind(const bal_socket* s, const char* addr, const char* srv);
bool bal_bindall(const bal_socket* s, const char* srv);

bool bal_listen(bal_socket* s, int backlog);
bool bal_accept(const bal_socket* s, bal_socket** res, bal_sockaddr* resaddr);

bool bal_get_option(const bal_socket* s, int level, int name, void* optval, socklen_t len);
bool bal_set_option(const bal_socket* s, int level, int name, const void* optval, socklen_t len);

bool bal_set_broadcast(const bal_socket* s, int value);
bool bal_get_broadcast(const bal_socket* s, int* value);

bool bal_set_debug(const bal_socket* s, int value);
bool bal_get_debug(const bal_socket* s, int* value);

bool bal_set_linger(const bal_socket* s, bal_linger sec);
bool bal_get_linger(const bal_socket* s, bal_linger* sec);

bool bal_set_keepalive(const bal_socket* s, int value);
bool bal_get_keepalive(const bal_socket* s, int* value);

bool bal_set_oobinline(const bal_socket* s, int value);
bool bal_get_oobinline(const bal_socket* s, int* value);

bool bal_set_reuseaddr(const bal_socket* s, int value);
bool bal_get_reuseaddr(const bal_socket* s, int* value);

bool bal_set_sendbuf_size(const bal_socket* s, int size);
bool bal_get_sendbuf_size(const bal_socket* s, int* size);

bool bal_set_recvbuf_size(const bal_socket* s, int size);
bool bal_get_recvbuf_size(const bal_socket* s, int* size);

bool bal_set_send_timeout(const bal_socket* s, bal_tvsec sec, bal_tvusec usec);
bool bal_get_send_timeout(const bal_socket* s, bal_tvsec* sec, bal_tvusec* usec);

bool bal_set_recv_timeout(const bal_socket* s, bal_tvsec sec, bal_tvusec usec);
bool bal_get_recv_timeout(const bal_socket* s, bal_tvsec* sec, bal_tvusec* usec);

int bal_get_sock_error(const bal_socket* s);
int bal_get_error(bal_error* err);
int bal_get_error_ext(bal_error* err);

bool bal_is_readable(const bal_socket* s);
bool bal_is_writable(const bal_socket* s);
bool bal_is_listening(const bal_socket* s);

bool bal_set_io_mode(const bal_socket* s, bool async);

size_t bal_get_recvqueue_size(const bal_socket* s);

bool bal_resolve_host(const char* host, bal_addrlist* out);
bool bal_get_peer_addr(const bal_socket* s, bal_sockaddr* out);
bool bal_get_peer_strings(const bal_socket* s, bool dns, bal_addrstrings* out);
bool bal_get_localhost_addr(const bal_socket* s, bal_sockaddr* out);
bool bal_get_localhost_strings(const bal_socket* s, bool dns, bal_addrstrings* out);
bool bal_get_addrstrings(const bal_sockaddr* in, bool dns, bal_addrstrings* out);

bool bal_reset_addrlist(bal_addrlist* addrs);
const bal_sockaddr* bal_enum_addrlist(bal_addrlist* addrs);
bool bal_free_addrlist(bal_addrlist* addrs);

void bal_thread_yield(void);
void bal_sleep_msec(uint32_t msec);

static inline
void bal_addtomask(bal_socket* s, uint32_t bits)
{
    if (_bal_okptr(s))
        bal_setbitshigh(&s->state.mask, bits);
}

static inline
void bal_remfrommask(bal_socket* s, uint32_t bits)
{
    if (_bal_okptr(s))
        bal_setbitslow(&s->state.mask, bits);
}

static inline
bool bal_bitsinmask(const bal_socket* s, uint32_t bits)
{
    return _bal_okptr(s) ? bal_isbitset(s->state.mask, bits) : false;
}

# if defined(__cplusplus)
}
# endif

#endif /* !_BAL_H_INCLUDED */
