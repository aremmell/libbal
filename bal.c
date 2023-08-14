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
#include "balinternal.h"

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

int bal_asyncselect(const bal_socket* s, bal_async_callback proc, uint32_t mask)
{
    return _bal_asyncselect(s, proc, mask);
}

int bal_autosocket(bal_socket* s, int af, int pt, const char* host, const char* port)
{
    int r = BAL_FALSE;

    if (s && _bal_validstr(host)) {
        int _af = ((af == 0) ? PF_UNSPEC : af);
        int _st = ((pt == 0) ? 0 : (pt == IPPROTO_TCP) ? SOCK_STREAM : SOCK_DGRAM);
        bal_addrinfo ai = {NULL, NULL};

        if (BAL_TRUE == _bal_getaddrinfo(0, _af, _st, host, port, &ai)) {
            const struct addrinfo* a = NULL;

            while (NULL != (a = _bal_enumaddrinfo(&ai))) {
                if (BAL_TRUE == bal_sock_create(s, a->ai_family, a->ai_protocol,
                    a->ai_socktype)) {
                    r = BAL_TRUE;
                    break;
                }
            }

            freeaddrinfo(ai._ai);
        }
    }

    return r;
}

int bal_sock_create(bal_socket* s, int af, int pt, int st)
{
    int r = BAL_FALSE;

    if (s) {
        bal_reset(s);
        s->sd = socket(af, st, pt);
        if (-1 == s->sd)
            _bal_setlasterror(errno);
        else
            r = BAL_TRUE;
    }

    return r;
}

void bal_reset(bal_socket* s)
{
    if (s) {
        s->af = 0;
        s->pf = 0;
        s->sd = BAL_BADSOCKET;
        s->st = 0;
        s->_f = 0U;
    }
}

int bal_close(bal_socket* s)
{
    BAL_ASSERT(NULL != s && BAL_BADSOCKET != s->sd);
    if (!_bal_validptr(s) || BAL_BADSOCKET == s->sd) {
        _bal_dbglog("error: not a valid socket");
        return BAL_FALSE;
    }

#if defined(__WIN__)
    if (SOCKET_ERROR == closesocket(s->sd)) {
        (void)_bal_handleerr(WSAGetLastError());
        return BAL_FALSE;
    }
#else
    if (-1 == close(s->sd)) {
        (void)_bal_handleerr(errno);
        return BAL_FALSE;
    }
#endif

    return BAL_TRUE;
}

int bal_shutdown(bal_socket* s, int how)
{
    int r = BAL_FALSE;

    if (_bal_validptr(s)) {
        r = shutdown(s->sd, how);
        if (0 == r) {
            if (how == BAL_SHUT_RDWR)
                s->_f &= ~(BAL_F_PENDCONN | BAL_F_LISTENING);
            else if (how == BAL_SHUT_RD)
                s->_f &= ~BAL_F_LISTENING;
            else if (how == BAL_SHUT_WR)
                s->_f &= ~BAL_F_PENDCONN;
        } else {
            (void)_bal_handleerr(errno);
        }
    }

    return r;
}

int bal_connect(const bal_socket* s, const char* host, const char* port)
{
    int r = BAL_FALSE;

    if (s && _bal_validstr(host) && _bal_validstr(port)) {
        bal_addrinfo ai = {NULL, NULL};

        if (BAL_TRUE == _bal_getaddrinfo(0, s->af, s->st, host, port, &ai)) {
            bal_addrlist al = {NULL, NULL};

            if (BAL_TRUE == _bal_aitoal(&ai, &al)) {
                r = bal_connectaddrlist((bal_socket*)s, &al);
                bal_freeaddrlist(&al);
            }

            freeaddrinfo(ai._ai);
        }
    }

    return r;
}

int bal_connectaddrlist(bal_socket* s, bal_addrlist* al)
{
    int r = BAL_FALSE;

    if (s) {
        if (BAL_TRUE == bal_resetaddrlist(al)) {
            const bal_sockaddr* sa = NULL;

            while (NULL != (sa = bal_enumaddrlist(al))) {
                r = connect(s->sd, (const struct sockaddr*)sa, _BAL_SASIZE(*sa));
#if defined(__WIN__)
                if (!r || WSAEWOULDBLOCK == WSAGetLastError()) {
#else
                if (!r || EAGAIN == errno || EINPROGRESS == errno) {
#endif
                    s->_f |= BAL_F_PENDCONN;
                    r = BAL_TRUE;
                    break;
                } else {
                    _bal_handleerr(errno);
                }
            }
        }
    }

    return r;
}

int bal_send(const bal_socket* s, const void* data, size_t len, int flags)
{
    if (s && data && len)
        return send(s->sd, data, len, flags);
    else
        return BAL_FALSE;
}

int bal_recv(const bal_socket* s, void* data, size_t len, int flags)
{
    if (s && data && len)
        return recv(s->sd, data, len, flags);
    else
        return BAL_FALSE;
}

int bal_sendto(const bal_socket* s, const char* host, const char* port,
    const void* data, size_t len, int flags)
{
    int r = BAL_FALSE;

    if (s && _bal_validstr(host) && _bal_validstr(port) && data && len) {
        bal_addrinfo ai = {NULL, NULL};

        if (BAL_TRUE == _bal_getaddrinfo(0, PF_UNSPEC, SOCK_DGRAM, host, port, &ai) &&
            NULL != ai._ai) {
            r = bal_sendtoaddr(s, (const bal_sockaddr*)ai._ai->ai_addr, data, len, flags);
            freeaddrinfo(ai._ai);
        }
    }

    return r;
}

int bal_sendtoaddr(const bal_socket* s, const bal_sockaddr* sa, const void* data,
    size_t len, int flags)
{
    if (s && sa && data && len)
        return sendto(s->sd, data, len, flags, (const struct sockaddr*)sa, _BAL_SASIZE(*sa));
    else
        return BAL_FALSE;
}

int bal_recvfrom(const bal_socket* s, void* data, size_t len, int flags, bal_sockaddr* res)
{
    if (s && data && len) {
        socklen_t sasize = sizeof(bal_sockaddr);
        return recvfrom(s->sd, (char*)data, len, flags, (struct sockaddr*)res, &sasize);
    } else {
        return BAL_FALSE;
    }
}

int bal_bind(const bal_socket* s, const char* addr, const char* port)
{
    int r = BAL_FALSE;

    if (s && _bal_validstr(addr) && _bal_validstr(port)) {
        bal_addrinfo ai = {NULL, NULL};

        if (BAL_TRUE == _bal_getaddrinfo(AI_NUMERICHOST, s->af, s->st, addr, port, &ai)) {
            const struct addrinfo* a = NULL;

            while (NULL != (a = _bal_enumaddrinfo(&ai))) {
                if (0 == bind(s->sd, (const struct sockaddr*)a->ai_addr, a->ai_addrlen)) {
                    r = BAL_TRUE;
                    break;
                }
            }

            freeaddrinfo(ai._ai);
        }
    }

    return r;
}

int bal_listen(bal_socket* s, int backlog)
{
    int r = BAL_FALSE;
    if (s) {
        r = listen(s->sd, backlog);
        if (0 == r)
            s->_f |= BAL_F_LISTENING;
    }

    return r;
}

int bal_accept(const bal_socket* s, bal_socket* res, bal_sockaddr* resaddr)
{
    int r = BAL_FALSE;

    if (s && res && resaddr) {
        socklen_t sasize = sizeof(bal_sockaddr);
        res->sd = accept(s->sd, (struct sockaddr*)resaddr, &sasize);
        if (res->sd > 0 ||
#if defined(__WIN__)
        WSAEWOULDBLOCK == WSAGetLastError())
#else
        EAGAIN == errno || EINPROGRESS == errno)
#endif
            r = BAL_TRUE;
    }

    return r;
}

int bal_getoption(const bal_socket* s, int level, int name, void* optval, socklen_t len)
{
    return (NULL != s) ? getsockopt(s->sd, level, name, optval, &len) : BAL_FALSE;
}

int bal_setoption(const bal_socket* s, int level, int name, const void* optval, socklen_t len)
{
    return (NULL != s) ? setsockopt(s->sd, level, name, optval, len) : BAL_FALSE;
}

int bal_setbroadcast(const bal_socket* s, int flag)
{
    return bal_setoption(s, SOL_SOCKET, SO_BROADCAST, &flag, sizeof(int));
}

int bal_getbroadcast(const bal_socket* s)
{
    int r = BAL_FALSE;

    if (BAL_FALSE == bal_getoption(s, SOL_SOCKET, SO_BROADCAST, &r, sizeof(int)))
        r = BAL_FALSE;

    return r;
}

int bal_setdebug(const bal_socket* s, int flag)
{
    return bal_setoption(s, SOL_SOCKET, SO_DEBUG, &flag, sizeof(int));
}

int bal_getdebug(const bal_socket* s)
{
    int r = BAL_FALSE;

    if (BAL_FALSE == bal_getoption(s, SOL_SOCKET, SO_DEBUG, &r, sizeof(int)))
        r = BAL_FALSE;

    return r;
}

int bal_setlinger(const bal_socket* s, int sec)
{
    struct linger l;

    l.l_onoff  = (0 != sec) ? 1 : 0;
    l.l_linger = sec;

    return bal_setoption(s, SOL_SOCKET, SO_LINGER, &l, sizeof(l));
}

int bal_getlinger(const bal_socket* s, int* sec)
{
    int r = BAL_FALSE;

    if (sec) {
        struct linger l = {0};

        if (BAL_TRUE == bal_getoption(s, SOL_SOCKET, SO_LINGER, &l, sizeof(l))) {
            *sec = l.l_linger;
            r    = BAL_TRUE;
        }
    }

    return r;
}

int bal_setkeepalive(const bal_socket* s, int flag)
{
    return bal_setoption(s, SOL_SOCKET, SO_KEEPALIVE, &flag, sizeof(int));
}

int bal_getkeepalive(const bal_socket* s)
{
    int r = BAL_FALSE;

    if (BAL_FALSE == bal_getoption(s, SOL_SOCKET, SO_KEEPALIVE, &r, sizeof(int)))
        r = BAL_FALSE;

    return r;
}

int bal_setoobinline(const bal_socket* s, int flag)
{
    return bal_setoption(s, SOL_SOCKET, SO_OOBINLINE, &flag, sizeof(int));
}

int bal_getoobinline(const bal_socket* s)
{
    int r = BAL_FALSE;

    if (BAL_FALSE == bal_getoption(s, SOL_SOCKET, SO_OOBINLINE, &r, sizeof(int)))
        r = BAL_FALSE;

    return r;
}

int bal_setreuseaddr(const bal_socket* s, int flag)
{
    return bal_setoption(s, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(int));
}

int bal_getreuseaddr(const bal_socket* s)
{
    int r = BAL_FALSE;

    if (BAL_FALSE == bal_getoption(s, SOL_SOCKET, SO_REUSEADDR, &r, sizeof(int)))
        r = BAL_FALSE;

    return r;
}

int bal_setsendbufsize(const bal_socket* s, int size)
{
    return bal_setoption(s, SOL_SOCKET, SO_SNDBUF, &size, sizeof(int));
}

int bal_getsendbufsize(const bal_socket* s)
{
    int r = BAL_FALSE;

    if (BAL_FALSE == bal_getoption(s, SOL_SOCKET, SO_SNDBUF, &r, sizeof(int)))
        r = BAL_FALSE;

    return r;
}

int bal_setrecvbufsize(const bal_socket* s, int size)
{
    return bal_setoption(s, SOL_SOCKET, SO_RCVBUF, &size, sizeof(int));
}

int bal_getrecvbufsize(const bal_socket* s)
{
    int r = BAL_FALSE;

    if (BAL_FALSE == bal_getoption(s, SOL_SOCKET, SO_RCVBUF, &r, sizeof(int)))
        r = BAL_FALSE;

    return r;
}

int bal_setsendtimeout(const bal_socket* s, long sec, long usec)
{
    struct timeval t = {sec, usec};
    return bal_setoption(s, SOL_SOCKET, SO_SNDTIMEO, &t, sizeof(t));
}

int bal_getsendtimeout(const bal_socket* s, long* sec, long* usec)
{
    int r = BAL_FALSE;

    if (sec && usec) {
        struct timeval t = {0};

        if (BAL_TRUE == bal_getoption(s, SOL_SOCKET, SO_SNDTIMEO, &t, sizeof(t))) {
            *sec  = t.tv_sec;
            *usec = t.tv_usec;
            r     = BAL_TRUE;
        }
    }

    return r;
}

int bal_setrecvtimeout(const bal_socket* s, long sec, long usec)
{
    struct timeval t = {sec, usec};
    return bal_setoption(s, SOL_SOCKET, SO_RCVTIMEO, &t, sizeof(t));
}

int bal_getrecvtimeout(const bal_socket* s, long* sec, long* usec)
{
    int r = BAL_FALSE;

    if (sec && usec) {
        struct timeval t = {0};

        if (BAL_TRUE == bal_getoption(s, SOL_SOCKET, SO_RCVTIMEO, &t, sizeof(t))) {
            *sec  = t.tv_sec;
            *usec = t.tv_usec;
            r     = BAL_TRUE;
        }
    }

    return r;
}

int bal_geterror(const bal_socket* s)
{
    int r = BAL_FALSE;

    if (BAL_FALSE == bal_getoption(s, SOL_SOCKET, SO_ERROR, &r, sizeof(int)))
        r = BAL_FALSE;

    return r;
}

bool bal_isreadable(const bal_socket* s)
{
    bool r = false;

    if (s) {
        fd_set fd        = {0};
        struct timeval t = {0};

        FD_ZERO(&fd);
        FD_SET(s->sd, &fd);

        if (0 == select(1, &fd, NULL, NULL, &t)) {
            if (FD_ISSET(s->sd, &fd))
                r = true;
        }
    }

    return r;
}

bool bal_iswritable(const bal_socket* s)
{
    bool r = false;

    if (s) {
        fd_set fd        = {0};
        struct timeval t = {0};

        FD_ZERO(&fd);
        FD_SET(s->sd, &fd);

        if (0 == select(1, NULL, &fd, NULL, &t)) {
            if (FD_ISSET(s->sd, &fd))
                r = true;
        }
    }

    return r;
}

int bal_setiomode(const bal_socket* s, bool async)
{
#if defined(__WIN__)
    unsigned long flag = async ? 1UL : 0UL;
    return ((NULL != s) ? ioctlsocket(s->sd, FIONBIO, &flag) : BAL_FALSE);
#else
    uint32_t flag = async ? O_NONBLOCK : 0U;
    return ((NULL != s) ? fcntl(s->sd, F_SETFL, flag) : BAL_FALSE);
#endif
}

size_t bal_recvqueuesize(const bal_socket* s)
{
    size_t r = 0UL;

    if (s) {
#if defined(__WIN__)
        if (0 != ioctlsocket(s->sd, FIONREAD, (void*)&r))
            r = 0UL;
#else
        if (0 != ioctl(s->sd, FIONREAD, &r))
            r = 0UL;
#endif
    }

    return r;
}

int bal_lastliberror(bal_error* err)
{
    return _bal_getlasterror(NULL, err);
}

int bal_lastsockerror(const bal_socket* s, bal_error* err)
{
    return _bal_getlasterror(s, err);
}

int bal_resolvehost(const char* host, bal_addrlist* out)
{
    int r = BAL_FALSE;

    if (_bal_validstr(host) && out) {
        bal_addrinfo ai = {NULL, NULL};

        int get = _bal_getaddrinfo(0, PF_UNSPEC, SOCK_STREAM, host, NULL, &ai);
        if (BAL_TRUE == get) {
            if (BAL_TRUE == _bal_aitoal(&ai, out))
                r = BAL_TRUE;
            freeaddrinfo(ai._ai);
        }
    }

    return r;
}

int bal_getremotehostaddr(const bal_socket* s, bal_sockaddr* out)
{
    int r = BAL_FALSE;

    if (s && out) {
        socklen_t salen = sizeof(bal_sockaddr);
        memset(out, 0, sizeof(bal_sockaddr));

        if (0 == getpeername(s->sd, (struct sockaddr*)out, &salen))
            r = BAL_TRUE;
    }

    return r;
}

int bal_getremotehoststrings(const bal_socket* s, int dns, bal_addrstrings* out)
{
    int r           = BAL_FALSE;
    bal_sockaddr sa = {0};

    if (BAL_TRUE == bal_getremotehostaddr(s, &sa))
        r = bal_getaddrstrings(&sa, dns, out);

    return r;
}

int bal_getlocalhostaddr(const bal_socket* s, bal_sockaddr* out)
{
    int r = BAL_FALSE;

    if (s && out) {
        socklen_t salen = sizeof(bal_sockaddr);
        memset(out, 0, sizeof(bal_sockaddr));

        if (0 == getsockname(s->sd, (struct sockaddr*)out, &salen))
            r = BAL_TRUE;
    }

    return r;
}

int bal_getlocalhoststrings(const bal_socket* s, int dns, bal_addrstrings* out)
{
    int r           = BAL_FALSE;
    bal_sockaddr sa = {0};

    if (BAL_TRUE == bal_getlocalhostaddr(s, &sa))
        r = bal_getaddrstrings(&sa, dns, out);

    return r;
}

int bal_resetaddrlist(bal_addrlist* al)
{
    int r = BAL_FALSE;

    if (al) {
        al->_p = al->_a;
        r      = BAL_TRUE;
    }

    return r;
}

const bal_sockaddr* bal_enumaddrlist(bal_addrlist* al)
{
    const bal_sockaddr* r = NULL;

    if (al && al->_a) {
        if (al->_p) {
            r      = &al->_p->_sa;
            al->_p = al->_p->_n;
        } else {
            bal_resetaddrlist(al);
        }
    }

    return r;
}

int bal_freeaddrlist(bal_addrlist* al)
{
    int r = BAL_FALSE;

    if (al) {
        bal_addr* a = NULL;
        al->_p = al->_a;

        while (al->_p) {
            a = al->_p->_n;
            _bal_safefree(&al->_p);
            al->_p = a;
        }

        r      = BAL_TRUE;
        al->_p = al->_a = NULL;
    }

    return r;
}

int bal_getaddrstrings(const bal_sockaddr* in, bool dns, bal_addrstrings* out)
{
    int r = BAL_FALSE;

    if (in && out) {
        memset(out, 0, sizeof(bal_addrstrings));

        if (BAL_TRUE == _bal_getnameinfo(_BAL_NI_NODNS, in, out->ip, out->port)) {
            if (dns) {
                int get = _bal_getnameinfo(_BAL_NI_DNS, in, out->host, out->port);
                if (BAL_FALSE == get)
                    (void)_bal_retstr(out->host, BAL_UNKNOWN, NI_MAXHOST);
            }

            if (PF_INET == ((struct sockaddr*)in)->sa_family)
                out->type = BAL_AS_IPV4;
            else if (PF_INET6 == ((struct sockaddr*)in)->sa_family)
                out->type = BAL_AS_IPV6;
            else
                out->type = BAL_UNKNOWN;

            r = BAL_TRUE;
        }
    }

    return r;
}

void bal_yield_thread(void)
{
#if defined(__WIN__)
    Sleep(1);
#else
    int yield = sched_yield();
    BAL_ASSERT_UNUSED(yield, 0 == yield);
#endif
}
