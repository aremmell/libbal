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
#include "balhelpers.h"

#if defined(__WIN__)
# pragma comment(lib, "ws2_32.lib")
#endif

/******************************************************************************\
 *                             Exported Functions                             *
\******************************************************************************/

int bal_autosocket(bal_socket** s, int addr_fam, int proto, const char* host,
    const char* port)
{
    int r = BAL_FALSE;

    if (_bal_validptrptr(s) && _bal_validstr(host)) {
        if (0 == addr_fam)
            addr_fam = PF_UNSPEC;

        int type = (proto == 0 ? 0 : (proto == IPPROTO_TCP ? SOCK_STREAM : SOCK_DGRAM));
        struct addrinfo* ai = NULL;

        if (BAL_TRUE == _bal_getaddrinfo(0, addr_fam, type, host, port, &ai) && NULL != ai) {
            struct addrinfo* cur = ai;
            do {
                if (BAL_TRUE == bal_sock_create(s, cur->ai_family, cur->ai_protocol,
                    cur->ai_socktype)) {
                    r = BAL_TRUE;
                    break;
                }
                cur = cur->ai_next;
            } while (NULL != cur);

            freeaddrinfo(ai);
        }
    }

    return r;
}

int bal_sock_create(bal_socket** s, int addr_fam, int type, int proto)
{
    int r = BAL_FALSE;

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
                r              = BAL_TRUE;
            }
        }
    }

    return r;
}

int bal_close(bal_socket** s, bool destroy)
{
    int closed    = BAL_FALSE;
    int destroyed = BAL_FALSE;

    if (_bal_validptrptr(s) && _bal_validsock(*s)) {
        BAL_ASSERT(BAL_BADSOCKET != (*s)->sd);
#if defined(__WIN__)
        if (SOCKET_ERROR == closesocket((*s)->sd)) {
            _bal_handlelasterr()
        }
#else
        if (-1 == close((*s)->sd)) {
            _bal_handlelasterr();
        }
#endif
        else {
            _bal_dbglog("closed socket "BAL_SOCKET_SPEC" (%p)", (*s)->sd, *s);
            (*s)->state.bits |= BAL_S_CLOSE;
            (*s)->state.bits &= ~(BAL_S_CONNECT | BAL_S_LISTEN);
            closed            = BAL_TRUE;
        }

        destroyed = destroy ? bal_sock_destroy(s) : BAL_FALSE;
    }

    return BAL_TRUE == closed && BAL_TRUE == destroyed ? BAL_TRUE : BAL_FALSE;
}

int bal_shutdown(bal_socket* s, int how)
{
    int r = BAL_FALSE;

    if (_bal_validsock(s)) {
        r = shutdown(s->sd, how);
        if (0 == r) {
            if (how == BAL_SHUT_RDWR)
                s->state.bits &= ~(BAL_S_CONNECT | BAL_S_LISTEN);
            else if (how == BAL_SHUT_RD) {
                s->state.bits &= ~BAL_S_LISTEN;
            } else if (how == BAL_SHUT_WR) {
                s->state.bits &= ~BAL_S_CONNECT;
            }
        } else {
            _bal_handlelasterr();
        }
    }

    return r;
}

int bal_connect(const bal_socket* s, const char* host, const char* port)
{
    int r = BAL_FALSE;

    if (s && _bal_validstr(host) && _bal_validstr(port)) {
        struct addrinfo* ai = NULL;

        if (BAL_TRUE == _bal_getaddrinfo(0, s->addr_fam, s->type, host, port, &ai)) {
            bal_addrlist al = {NULL, NULL};

            if (BAL_TRUE == _bal_aitoal(ai, &al)) {
                r = bal_connectaddrlist((bal_socket*)s, &al);
                bal_freeaddrlist(&al);
            }

            freeaddrinfo(ai);
        }
    }

    return r;
}

int bal_connectaddrlist(bal_socket* s, bal_addrlist* al)
{
    int r = BAL_FALSE;

    if (_bal_validsock(s)) {
        if (BAL_TRUE == bal_resetaddrlist(al)) {
            const bal_sockaddr* sa = NULL;

            while (NULL != (sa = bal_enumaddrlist(al))) {
                r = connect(s->sd, (const struct sockaddr*)sa, _BAL_SASIZE(*sa));
#if defined(__WIN__)
                if (!r || WSAEWOULDBLOCK == WSAGetLastError()) {
#else
                if (!r || EAGAIN == errno || EINPROGRESS == errno) {
#endif
                    bal_setbitshigh(&s->state.mask, BAL_E_WRITE);
                    bal_setbitshigh(&s->state.bits, BAL_S_CONNECT);
                    r = BAL_TRUE;
                    break;
                } else {
                    _bal_handlelasterr();
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

int bal_sendto(const bal_socket* s, const char* host, const char* port, const void* data,
    size_t len, int flags)
{
    int r = BAL_FALSE;

    if (s && _bal_validstr(host) && _bal_validstr(port) && data && len) {
        struct addrinfo* ai = NULL;

        if (BAL_TRUE == _bal_getaddrinfo(0, PF_UNSPEC, SOCK_DGRAM, host, port, &ai) &&
            NULL != ai) {
            r = bal_sendtoaddr(s, (const bal_sockaddr*)ai->ai_addr, data, len, flags);
            freeaddrinfo(ai);
        }
    }

    return r;
}

int bal_sendtoaddr(const bal_socket* s, const bal_sockaddr* sa, const void* data,
    size_t len, int flags)
{
    if (s && sa && data && len)
        return sendto(s->sd, data, len, flags, (const struct sockaddr*)sa,
            _BAL_SASIZE(*sa));
    else
        return BAL_FALSE;
}

int bal_recvfrom(const bal_socket* s, void* data, size_t len, int flags,
    bal_sockaddr* res)
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
        struct addrinfo* ai = NULL;

        if (BAL_TRUE == _bal_getaddrinfo(AI_NUMERICHOST, s->addr_fam, s->type,
            addr, port, &ai) && NULL != ai) {
            struct addrinfo* cur = ai;
            do {
                if (0 == bind(s->sd, (const struct sockaddr*)cur->ai_addr,
                    cur->ai_addrlen)) {
                    r = BAL_TRUE;
                    break;
                }
                cur = cur->ai_next;
            } while (NULL != cur);

            freeaddrinfo(ai);
        }
    }

    return r;
}

int bal_listen(bal_socket* s, int backlog)
{
    int r = BAL_FALSE;

    if (s) {
        r = listen(s->sd, backlog);
        if (0 == r) {
            s->state.mask |= BAL_S_LISTEN;
        } else {
            _bal_handlelasterr();
        }
    }

    return r;
}

int bal_accept(const bal_socket* s, bal_socket** res, bal_sockaddr* resaddr)
{
    int r = BAL_FALSE;

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
                r                = BAL_TRUE;
            } else {
                _bal_handlelasterr();
                _bal_safefree(res);
            }
        }
    }

    return r;
}

int bal_getoption(const bal_socket* s, int level, int name, void* optval, socklen_t len)
{
    return (NULL != s) ? getsockopt(s->sd, level, name, optval, &len) : BAL_FALSE;
}

int bal_setoption(const bal_socket* s, int level, int name, const void* optval,
    socklen_t len)
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

    l.l_onoff = (0 != sec) ? 1 : 0;
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
            r = BAL_TRUE;
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
            *sec = t.tv_sec;
            *usec = t.tv_usec;
            r = BAL_TRUE;
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
            *sec = t.tv_sec;
            *usec = t.tv_usec;
            r = BAL_TRUE;
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

    if (_bal_validsock(s)) {
        fd_set fd        = {0};
        struct timeval t = {0};

        FD_ZERO(&fd);
        FD_SET(s->sd, &fd);

        if (0 == select(s->sd + 1, &fd, NULL, NULL, &t) &&
            FD_ISSET(s->sd, &fd)) {
            r = true;
        }
    }

    return r;
}

bool bal_iswritable(const bal_socket* s)
{
    bool r = false;

    if (_bal_validsock(s)) {
        fd_set fd        = {0};
        struct timeval t = {0};

        FD_ZERO(&fd);
        FD_SET(s->sd, &fd);

        if (0 == select(s->sd + 1, NULL, &fd, NULL, &t) &&
            FD_ISSET(s->sd, &fd)) {
            r = true;
        }
    }

    return r;
}

bool bal_islistening(const bal_socket* s)
{
    /* prefer using the OS, since technically the socket could have been created
     * elsewhere and assigned to a bal_socket. */
#if defined(__HAVE_SO_ACCEPTCONN__)
    int flag      = 0;
    int get       = bal_getoption(s, SOL_SOCKET, SO_ACCEPTCONN, &flag, sizeof(int));

    if (0 != get)
        _bal_handlelasterr();

    return 0 == get && 0 != flag;
#else
    /* backup method: bitmask. */
    return _bal_validsock(s) && bal_isbitset(s->state.mask, BAL_S_LISTEN);
#endif
}

int bal_setiomode(const bal_socket* s, bool async)
{
#if defined(__WIN__)
    unsigned long flag = async ? 1UL : 0UL;
    return (NULL != s) ? ioctlsocket(s->sd, FIONBIO, &flag) : BAL_FALSE;
#else
    uint32_t flag = async ? O_NONBLOCK : 0U;
    return (NULL != s) ? fcntl(s->sd, F_SETFL, flag) : BAL_FALSE;
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

int bal_getlasterror(bal_error* err)
{
    return _bal_getlasterror(err);
}

int bal_resolvehost(const char* host, bal_addrlist* out)
{
    int r = BAL_FALSE;

    if (_bal_validstr(host) && _bal_validptr(out)) {
        struct addrinfo* ai = NULL;
        int get = _bal_getaddrinfo(0, PF_UNSPEC, SOCK_STREAM, host, NULL, &ai);
        if (BAL_TRUE == get && NULL != ai) {
            if (BAL_TRUE == _bal_aitoal(ai, out))
                r = BAL_TRUE;
            freeaddrinfo(ai);
        }
    }

    return r;
}

int bal_getremotehostaddr(const bal_socket* s, bal_sockaddr* out)
{
    int r = BAL_FALSE;

    if (_bal_validsock(s) && _bal_validptr(out)) {
        memset(out, 0, sizeof(bal_sockaddr));

        socklen_t salen = sizeof(bal_sockaddr);
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

    if (_bal_validsock(s) && _bal_validptr(out)) {
        socklen_t salen = sizeof(bal_sockaddr);
        memset(out, 0, sizeof(bal_sockaddr));

        if (0 == getsockname(s->sd, (struct sockaddr*)out, &salen))
            r = BAL_TRUE;
    }

    return r;
}

int bal_getlocalhoststrings(const bal_socket* s, int dns, bal_addrstrings* out)
{
    int r = BAL_FALSE;
    bal_sockaddr sa = {0};

    if (BAL_TRUE == bal_getlocalhostaddr(s, &sa))
        r = bal_getaddrstrings(&sa, dns, out);

    return r;
}

int bal_getaddrstrings(const bal_sockaddr* in, bool dns, bal_addrstrings* out)
{
    int r = BAL_FALSE;

    if (_bal_validptr(in) && _bal_validptr(out)) {
        memset(out, 0, sizeof(bal_addrstrings));

        int get = _bal_getnameinfo(_BAL_NI_NODNS, in, out->addr, out->port);
        if (BAL_TRUE == get) {
            if (dns) {
                get = _bal_getnameinfo(_BAL_NI_DNS, in, out->host, out->port);
                if (BAL_FALSE == get)
                    (void)strncpy(out->host, BAL_UNKNOWN, NI_MAXHOST);
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

int bal_resetaddrlist(bal_addrlist* al)
{
    int r = BAL_FALSE;

    if (al) {
        al->iter = al->addr;
        r = BAL_TRUE;
    }

    return r;
}

const bal_sockaddr* bal_enumaddrlist(bal_addrlist* al)
{
    const bal_sockaddr* r = NULL;

    if (al && al->addr) {
        if (al->iter) {
            r = &al->iter->addr;
            al->iter = al->iter->next;
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
        al->iter = al->addr;

        while (al->iter) {
            a = al->iter->next;
            _bal_safefree(&al->iter);
            al->iter = a;
        }

        r      = BAL_TRUE;
        al->iter = al->addr = NULL;
    }

    return r;
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
