/*
 * bal.c
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
#include "bal.h"
#include "bal/internal.h"
#include "bal/helpers.h"

#if defined(__WIN__)
# pragma comment(lib, "ws2_32.lib")
#endif

/******************************************************************************\
 *                             Exported Functions                             *
\******************************************************************************/

bool bal_init(void)
{
    return _bal_init();
}

bool bal_cleanup(void)
{
    return _bal_cleanup();
}

bool bal_async_poll(bal_socket* s, bal_async_cb proc, uint32_t mask)
{
    return _bal_async_poll(s, proc, mask);
}

bool bal_auto_socket(bal_socket** s, int addr_fam, int proto, const char* host,
    const char* srv)
{
    bool retval = false;

    if (_bal_validptrptr(s) && _bal_validstr(host)) {
        struct addrinfo* ai = NULL;
        int type = (proto == 0 ? 0 : (proto == IPPROTO_TCP ? SOCK_STREAM : SOCK_DGRAM));
        bool get = _bal_get_addrinfo(0, addr_fam, type, host, srv, &ai);
        if (get && NULL != ai) {
            struct addrinfo* cur = ai;
            do {
                if (bal_sock_create(s, cur->ai_family, cur->ai_protocol,
                    cur->ai_socktype)) {
                    retval = true;
                    break;
                }
                cur = cur->ai_next;
            } while (NULL != cur);

            freeaddrinfo(ai);
        }
    }

    return retval;
}

bool bal_sock_create(bal_socket** s, int addr_fam, int type, int proto)
{
    bool retval = false;

    if (_bal_validptrptr(s)) {
        *s = calloc(1UL, sizeof(bal_socket));
        if (!_bal_validptr(*s)) {
            _bal_handlelasterr();
        } else {
            (*s)->sd = socket(addr_fam, type, proto);
            if (-1 == (*s)->sd) {
                _bal_handlelasterr();
                _bal_safefree(s);
            } else {
                (*s)->addr_fam = addr_fam;
                (*s)->type     = type;
                (*s)->proto    = proto;
                retval         = true;
            }
        }
    }

    return retval;
}

void bal_sock_destroy(bal_socket** s)
{
    _bal_sock_destroy(s);
}

bool bal_close(bal_socket** s, bool destroy)
{
    bool retval = false;

    if (_bal_validptrptr(s) && _bal_validsock(*s)) {
#if defined(__WIN__)
        if (SOCKET_ERROR == closesocket((*s)->sd)) {
            _bal_handlelasterr();
        }
#else
        if (-1 == close((*s)->sd)) {
            _bal_handlelasterr();
        }
#endif
        else {
            _bal_dbglog("closed socket "BAL_SOCKET_SPEC" (%p, mask = %08"PRIx32")",
                (*s)->sd, *s, (*s)->state.mask);
            bal_setbitshigh(&(*s)->state.bits, BAL_S_CLOSE);
            bal_setbitslow(&(*s)->state.bits, BAL_S_CONNECT | BAL_S_LISTEN);
            retval = true;
        }

        if (destroy)
            bal_sock_destroy(s);
    }

    return retval;
}

bool bal_shutdown(bal_socket* s, int how)
{
    bool retval = false;

    if (_bal_validsock(s)) {
        if (-1 == shutdown(s->sd, how)) {
            _bal_handlelasterr();
        } else {
            if (how == BAL_SHUT_RDWR) {
                bal_setbitslow(&s->state.mask, BAL_EVT_READ | BAL_EVT_WRITE);
                bal_setbitslow(&s->state.bits, BAL_S_CONNECT | BAL_S_LISTEN);
            } else if (how == BAL_SHUT_RD) {
                bal_setbitslow(&s->state.mask, BAL_EVT_READ);
                bal_setbitslow(&s->state.bits, BAL_S_LISTEN);
            } else if (how == BAL_SHUT_WR) {
                bal_setbitslow(&s->state.mask, BAL_EVT_WRITE);
                bal_setbitslow(&s->state.bits, BAL_S_CONNECT);
            }
            retval = true;
        }
    }

    return retval;
}

bool bal_connect(bal_socket* s, const char* host, const char* port)
{
    bool retval = false;

    if (_bal_validsock(s) && _bal_validstr(host) && _bal_validstr(port)) {
        struct addrinfo* ai = NULL;
        if (_bal_get_addrinfo(0, s->addr_fam, s->type, host, port, &ai)) {
            bal_addrlist al = {NULL, NULL};
            if (_bal_addrinfo_to_addrlist(ai, &al)) {
                retval = bal_connect_addrlist(s, &al);
                bal_free_addrlist(&al);
            }
            freeaddrinfo(ai);
        }
    }

    return retval;
}

bool bal_connect_addrlist(bal_socket* s, bal_addrlist* al)
{
    bool retval = false;

    if (_bal_validsock(s)) {
        if (bal_reset_addrlist(al)) {
            const bal_sockaddr* sa = NULL;
            while (NULL != (sa = bal_enum_addrlist(al))) {
                int ret = connect(s->sd, (const struct sockaddr*)sa, _BAL_SASIZE(*sa));
#if defined(__WIN__)
                if (!ret || WSAEWOULDBLOCK == WSAGetLastError()) {
#else
                if (!ret || EAGAIN == errno || EINPROGRESS == errno) {
#endif
                    bal_setbitshigh(&s->state.mask, BAL_EVT_WRITE);
                    bal_setbitshigh(&s->state.bits, BAL_S_CONNECT);
                    retval = true;
                    break;
                } else {
                    _bal_handlelasterr();
                }
            }
        }
    }

    return retval;
}

int bal_send(const bal_socket* s, const void* data, bal_iolen len, int flags)
{
    int sent = -1;

    if (_bal_validsock(s) && _bal_validptr(data) && _bal_validlen(len)) {
        sent = send(s->sd, data, len, flags);
        if (-1 == sent)
            _bal_handlelasterr();
    }

    return sent;
}

int bal_recv(const bal_socket* s, void* data, bal_iolen len, int flags)
{
    int read = -1;

    if (_bal_validsock(s) && _bal_validptr(data) && _bal_validlen(len)) {
        read = recv(s->sd, data, len, flags);
        if (0 >= read)
            _bal_handlelasterr();
    }

    return read;
}

int bal_sendto(const bal_socket* s, const char* host, const char* port,
    const void* data, bal_iolen len, int flags)
{
    int sent = -1;

    if (_bal_validsock(s) && _bal_validstr(host) && _bal_validstr(port) &&
        _bal_validptr(data) && _bal_validlen(len)) {
        struct addrinfo* ai = NULL;
        int gai_flags = AI_NUMERICSERV;
        bool get = _bal_get_addrinfo(gai_flags, PF_UNSPEC, SOCK_DGRAM, host, port, &ai);
        if (get && NULL != ai) {
            sent = bal_sendto_addr(s, (const bal_sockaddr*)ai->ai_addr, data, len, flags);
            freeaddrinfo(ai);
        }
    }

    return sent;
}

int bal_sendto_addr(const bal_socket* s, const bal_sockaddr* sa, const void* data,
    bal_iolen len, int flags)
{
    int sent = -1;

    if (_bal_validsock(s) && _bal_validptr(sa) && _bal_validptr(data) && _bal_validlen(len)) {
        sent = sendto(s->sd, data, len, flags, (const struct sockaddr*)sa, _BAL_SASIZE(*sa));
        if (-1 == sent)
            _bal_handlelasterr();
    }

   return sent;
}

int bal_recvfrom(const bal_socket* s, void* data, bal_iolen len, int flags,
    bal_sockaddr* res)
{
    int read = -1;

    if (_bal_validsock(s) && _bal_validptr(data) && _bal_validlen(len)) {
        socklen_t sasize = sizeof(bal_sockaddr);
        read = recvfrom(s->sd, data, len, flags, (struct sockaddr*)res, &sasize);
        if (0 >= read)
            _bal_handlelasterr();
    }

    return read;
}

bool bal_bind(const bal_socket* s, const char* addr, const char* srv)
{
    bool retval = false;

    if (_bal_validsock(s) && _bal_validstr(addr) && _bal_validstr(srv)) {
        struct addrinfo* ai = NULL;
        bool get = _bal_get_addrinfo(AI_NUMERICHOST, s->addr_fam, s->type, addr, srv, &ai);
        if (get && NULL != ai) {
            struct addrinfo* cur = ai;
            do {
                int ret = bind(s->sd, (const struct sockaddr*)cur->ai_addr,
                    (bal_iolen)cur->ai_addrlen);
                if (0 == ret) {
                    retval = true;
                    break;
                } else {
                    _bal_handlelasterr();
                }
                cur = cur->ai_next;
            } while (NULL != cur);

            freeaddrinfo(ai);
        }
    }

    return retval;
}

bool bal_bindall(const bal_socket* s, const char* srv)
{
    bool retval = false;

    if (_bal_validsock(s) && _bal_validstr(srv)) {
        struct addrinfo* ai = NULL;
        int flags = AI_PASSIVE | AI_NUMERICHOST;
        bool get = _bal_get_addrinfo(flags, s->addr_fam, s->type, NULL, srv, &ai);
        if (get && NULL != ai) {
            int ret = bind(s->sd, (const struct sockaddr*)ai->ai_addr,
                (bal_iolen)ai->ai_addrlen);
            if (0 != ret)
                _bal_handlelasterr();
            retval = 0 == ret;
        }
    }

    return retval;
}

bool bal_listen(bal_socket* s, int backlog)
{
    bool retval = false;

    if (_bal_validsock(s)) {
        if (0 == listen(s->sd, backlog)) {
            bal_setbitshigh(&s->state.mask, BAL_EVT_READ);
            bal_setbitshigh(&s->state.bits, BAL_S_LISTEN);
            retval = true;
        } else {
            _bal_handlelasterr();
        }
    }

    return retval;
}

bool bal_accept(const bal_socket* s, bal_socket** res, bal_sockaddr* resaddr)
{
    bool retval = false;

    if (_bal_validsock(s) && _bal_validptrptr(res) && _bal_validptr(resaddr)) {
        *res = calloc(1UL, sizeof(bal_socket));
        if (!_bal_validptr(*res)) {
            _bal_handlelasterr();
        } else {
            socklen_t sasize = sizeof(bal_sockaddr);
            bal_descriptor sd = accept(s->sd, (struct sockaddr*)resaddr, &sasize);
            if (sd > 0) {
                (*res)->sd       = sd;
                (*res)->addr_fam = s->addr_fam;
                (*res)->type     = s->type;
                (*res)->proto    = s->proto;
                retval           = true;
            } else {
                _bal_handlelasterr();
                _bal_safefree(res);
            }
        }
    }

    return retval;
}

bool bal_get_option(const bal_socket* s, int level, int name, void* optval, socklen_t len)
{
    bool retval = false;

    if (_bal_validsock(s) && _bal_validptr(optval) && _bal_validlen(len)) {
        int get = getsockopt(s->sd, level, name, optval, &len);
        if (-1 == get)
            _bal_handlelasterr();
        retval = 0 == get;
    }

    return retval;
}

bool bal_set_option(const bal_socket* s, int level, int name, const void* optval,
    socklen_t len)
{
    bool retval = false;

    if (_bal_validsock(s) && _bal_validptr(optval) && _bal_validlen(len)) {
        int set = setsockopt(s->sd, level, name, optval, len);
        if (-1 == set)
            _bal_handlelasterr();
        retval = 0 == set;
    }

    return retval;
}

bool bal_set_broadcast(const bal_socket* s, int value)
{
    return bal_set_option(s, SOL_SOCKET, SO_BROADCAST, &value, sizeof(int));
}

bool bal_get_broadcast(const bal_socket* s, int* value)
{
    return bal_get_option(s, SOL_SOCKET, SO_BROADCAST, value, sizeof(int));
}

bool bal_set_debug(const bal_socket* s, int value)
{
    return bal_set_option(s, SOL_SOCKET, SO_DEBUG, &value, sizeof(int));
}

bool bal_get_debug(const bal_socket* s, int* value)
{
    return bal_get_option(s, SOL_SOCKET, SO_DEBUG, value, sizeof(int));
}

bool bal_set_linger(const bal_socket* s, bal_linger sec)
{
    struct linger l = { (0 != sec) ? 1 : 0, sec };
    return bal_set_option(s, SOL_SOCKET, SO_LINGER, &l, sizeof(struct linger));
}

bool bal_get_linger(const bal_socket* s, bal_linger* sec)
{
    bool retval = false;

    if (_bal_validptr(sec)) {
        struct linger l = {0};
        if (bal_get_option(s, SOL_SOCKET, SO_LINGER, &l, sizeof(struct linger))) {
            *sec   = l.l_linger;
            retval = true;
        }
    }

    return retval;
}

bool bal_set_keepalive(const bal_socket* s, int value)
{
    return bal_set_option(s, SOL_SOCKET, SO_KEEPALIVE, &value, sizeof(int));
}

bool bal_get_keepalive(const bal_socket* s, int* value)
{
    return bal_get_option(s, SOL_SOCKET, SO_KEEPALIVE, value, sizeof(int));
}

bool bal_set_oobinline(const bal_socket* s, int value)
{
    return bal_set_option(s, SOL_SOCKET, SO_OOBINLINE, &value, sizeof(int));
}

bool bal_get_oobinline(const bal_socket* s, int* value)
{
    return bal_get_option(s, SOL_SOCKET, SO_OOBINLINE, value, sizeof(int));
}

bool bal_set_reuseaddr(const bal_socket* s, int value)
{
    return bal_set_option(s, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(int));
}

bool bal_get_reuseaddr(const bal_socket* s, int* value)
{
    return bal_get_option(s, SOL_SOCKET, SO_REUSEADDR, value, sizeof(int));
}

bool bal_set_sendbuf_size(const bal_socket* s, int size)
{
    return bal_set_option(s, SOL_SOCKET, SO_SNDBUF, &size, sizeof(int));
}

bool bal_get_sendbuf_size(const bal_socket* s, int* size)
{
    return bal_get_option(s, SOL_SOCKET, SO_SNDBUF, size, sizeof(int));
}

bool bal_set_recvbuf_size(const bal_socket* s, int size)
{
    return bal_set_option(s, SOL_SOCKET, SO_RCVBUF, &size, sizeof(int));
}

bool bal_get_recvbuf_size(const bal_socket* s, int* size)
{
    return bal_get_option(s, SOL_SOCKET, SO_RCVBUF, size, sizeof(int));
}

bool bal_set_send_timeout(const bal_socket* s, long sec, long usec)
{
    struct timeval t = {sec, usec};
    return bal_set_option(s, SOL_SOCKET, SO_SNDTIMEO, &t, sizeof(struct timeval));
}

bool bal_get_send_timeout(const bal_socket* s, long* sec, long* usec)
{
    bool retval = false;

    if (_bal_validptr(sec) && _bal_validptr(usec)) {
        struct timeval t = {0};
        bool get = bal_get_option(s, SOL_SOCKET, SO_SNDTIMEO, &t, sizeof(struct timeval));
        if (get) {
            *sec  = t.tv_sec;
            *usec = t.tv_usec;
        }
        retval = get;
    }

    return retval;
}

bool bal_set_recv_timeout(const bal_socket* s, long sec, long usec)
{
    struct timeval t = {sec, usec};
    return bal_set_option(s, SOL_SOCKET, SO_RCVTIMEO, &t, sizeof(struct timeval));
}

bool bal_get_recv_timeout(const bal_socket* s, long* sec, long* usec)
{
    bool retval = false;

    if (_bal_validptr(sec) && _bal_validptr(usec)) {
        struct timeval t = {0};
        bool get = bal_get_option(s, SOL_SOCKET, SO_RCVTIMEO, &t, sizeof(struct timeval));
        if (get) {
            *sec  = t.tv_sec;
            *usec = t.tv_usec;
        }
        retval = get;
    }

    return retval;
}

int bal_get_error(const bal_socket* s)
{
    int err = 0;

    if (!bal_get_option(s, SOL_SOCKET, SO_ERROR, &err, sizeof(int))) {
#if defined(__WIN__)
        err = WSAGetLastError();
#else
        err = errno;
#endif
    }

    return err;
}

int bal_get_last_error(const bal_socket* s, bal_error* err)
{
    return _bal_get_last_error(s, err);
}

bool bal_is_readable(const bal_socket* s)
{
    bool r = false;

    if (_bal_validsock(s)) {
        fd_set fd        = {0};
        struct timeval t = {0};
#if defined(__WIN__)
        int high_sd = (int)s->sd + 1;
#else
        int high_sd = s->sd + 1;
#endif

        FD_ZERO(&fd);
        FD_SET(s->sd, &fd);

        if (0 == select(high_sd, &fd, NULL, NULL, &t) &&
            FD_ISSET(s->sd, &fd)) {
            r = true;
        }
    }

    return r;
}

bool bal_is_writable(const bal_socket* s)
{
    bool r = false;

    if (_bal_validsock(s)) {
        fd_set fd        = {0};
        struct timeval t = {0};
#if defined(__WIN__)
        int high_sd = (int)s->sd + 1;
#else
        int high_sd = s->sd + 1;
#endif

        FD_ZERO(&fd);
        FD_SET(s->sd, &fd);

        if (0 == select(high_sd, NULL, &fd, NULL, &t) &&
            FD_ISSET(s->sd, &fd)) {
            r = true;
        }
    }

    return r;
}

bool bal_is_listening(const bal_socket* s)
{
#if defined(__HAVE_SO_ACCEPTCONN__)
    /* prefer using the OS, since technically the socket could have been created
     * elsewhere and assigned to a bal_socket. */
    int flag = 0;
    return bal_get_option(s, SOL_SOCKET, SO_ACCEPTCONN, &flag, sizeof(int)) && 0 != flag;
#else
    /* backup method: bitmask. */
    return _bal_validsock(s) && bal_isbitset(s->state.bits, BAL_S_LISTEN);
#endif
}

bool bal_set_io_mode(const bal_socket* s, bool async)
{
    bool retval = false;

    if (_bal_validsock(s)) {
#if defined(__WIN__)
        u_long flag = async ? 1UL : 0UL;
        int ret = ioctlsocket(s->sd, FIONBIO, &flag);
        if (-1 == ret)
            _bal_handlelasterr();
        retval = 0 == ret;
#else
        int ret = fcntl(s->sd, F_SETFL, async ? O_NONBLOCK : 0U);
        if (-1 == ret)
            _bal_handlelasterr();
        retval = 0 == ret;
#endif
    }

    return retval;
}

size_t bal_get_recvqueue_size(const bal_socket* s)
{
    size_t size = 0UL;

    if (_bal_validsock(s)) {
#if defined(__WIN__)
        if (0 != ioctlsocket(s->sd, FIONREAD, (void*)&size)) {
            size = 0UL;
            _bal_handlelasterr();
        }
#else
        if (0 != ioctl(s->sd, FIONREAD, &size)) {
            size = 0UL;
            _bal_handlelasterr();
        }
#endif
    }

    return size;
}

bool bal_resolve_host(const char* host, bal_addrlist* out)
{
    bool retval = false;

    if (_bal_validstr(host) && _bal_validptr(out)) {
        struct addrinfo* ai = NULL;
        bool get = _bal_get_addrinfo(0, PF_UNSPEC, SOCK_STREAM, host, NULL, &ai);
        if (get && NULL != ai) {
            retval = _bal_addrinfo_to_addrlist(ai, out);
            freeaddrinfo(ai);
        }
    }

    return retval;
}

bool bal_get_peer_addr(const bal_socket* s, bal_sockaddr* out)
{
    bool retval = false;

    if (_bal_validsock(s) && _bal_validptr(out)) {
        memset(out, 0, sizeof(bal_sockaddr));
        socklen_t salen = sizeof(bal_sockaddr);
        int get = getpeername(s->sd, (struct sockaddr*)out, &salen);
        if (-1 == get)
            _bal_handlelasterr();
        retval = 0 == get;
    }

    return retval;
}

bool bal_get_peer_strings(const bal_socket* s, bool dns, bal_addrstrings* out)
{
    bal_sockaddr sa = {0};
    return bal_get_peer_addr(s, &sa) && bal_get_addrstrings(&sa, dns, out);
}

bool bal_get_localhost_addr(const bal_socket* s, bal_sockaddr* out)
{
    bool retval = false;

    if (_bal_validsock(s) && _bal_validptr(out)) {
        socklen_t salen = sizeof(bal_sockaddr);
        memset(out, 0, sizeof(bal_sockaddr));
        int get = getsockname(s->sd, (struct sockaddr*)out, &salen);
        if (-1 == get)
            _bal_handlelasterr();
        retval = 0 == get;
    }

    return retval;
}

bool bal_get_localhost_strings(const bal_socket* s, bool dns, bal_addrstrings* out)
{
    bal_sockaddr sa = {0};
    return bal_get_localhost_addr(s, &sa) && bal_get_addrstrings(&sa, dns, out);
}

bool bal_get_addrstrings(const bal_sockaddr* in, bool dns, bal_addrstrings* out)
{
    bool retval = false;

    if (_bal_validptr(in) && _bal_validptr(out)) {
        memset(out, 0, sizeof(bal_addrstrings));
        bool get = _bal_getnameinfo(_BAL_NI_NODNS, in, out->addr, out->port);
        if (get) {
            if (dns && _bal_getnameinfo(_BAL_NI_DNS, in, out->host, out->port))
                _bal_strcpy(out->host, NI_MAXHOST, BAL_UNKNOWN, sizeof(BAL_UNKNOWN));
            if (PF_INET == ((struct sockaddr*)in)->sa_family)
                out->type = BAL_AS_IPV4;
            else if (PF_INET6 == ((struct sockaddr*)in)->sa_family)
                out->type = BAL_AS_IPV6;
            else
                out->type = BAL_UNKNOWN;
            retval = true;
        }
    }

    return retval;
}

bool bal_reset_addrlist(bal_addrlist* addrs)
{
    bool retval = false;

    if (_bal_validptr(addrs)) {
        addrs->iter = addrs->addr;
        retval = true;
    }

    return retval;
}

const bal_sockaddr* bal_enum_addrlist(bal_addrlist* addrs)
{
    const bal_sockaddr* r = NULL;

    if (_bal_validptr(addrs) && _bal_validptr(addrs->addr)) {
        if (NULL != addrs->iter) {
            r        = &addrs->iter->addr;
            addrs->iter = addrs->iter->next;
        } else {
            bal_reset_addrlist(addrs);
        }
    }

    return r;
}

bool bal_free_addrlist(bal_addrlist* addrs)
{
    bool retval = false;

    if (_bal_validptr(addrs)) {
        bal_addr* a = NULL;
        addrs->iter = addrs->addr;

        while (NULL != addrs->iter) {
            a = addrs->iter->next;
            _bal_safefree(&addrs->iter);
            addrs->iter = a;
        }

        addrs->addr = NULL;
        retval   = true;
    }

    return retval;
}

void bal_thread_yield(void)
{
#if defined(__WIN__)
    (void)SwitchToThread();
#else
# if defined(_POSIX_PRIORITY_SCHEDULING)
    (void)sched_yield();
# else
    (void)pthread_yield();
# endif
#endif
}

void bal_sleep_msec(uint32_t msec)
{
    if (0U == msec)
        return;

#if defined(__WIN__)
    (void)SleepEx((DWORD)msec, TRUE);
#else
    struct timespec ts = { msec / 1000, (msec % 1000) * 1000000 };
    (void)nanosleep(&ts, NULL);
#endif
}
