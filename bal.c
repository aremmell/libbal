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

/*─────────────────────────────────────────────────────────────────────────────╮
│                            Exported Functions                                │
╰─────────────────────────────────────────────────────────────────────────────*/

int bal_initialize(void)
{
    int r = BAL_FALSE;

#if defined(__WIN__)
    WORD wVer  = MAKEWORD(WSOCK_MINVER, WSOCK_MAJVER);
    WSADATA wd = {0};

    if (0 == WSAStartup(wVer, &wd))
        r = BAL_TRUE;
#else
    r = BAL_TRUE;
#endif

    return r;
}

int bal_finalize(void)
{
    int r = BAL_FALSE;

#if defined(__WIN__)
    if (0 == WSACleanup())
        r = bal_asyncselect(NULL, NULL, BAL_S_DIE);
#else
    r = bal_asyncselect(NULL, NULL, BAL_S_DIE);
#endif

    return r;
}

int bal_asyncselect(const bal_socket* s, bal_async_callback proc, uint32_t mask)
{
    int r                          = BAL_FALSE;
    static bal_selectdata_list l   = {0};
    static bal_eventthread_data td = {0};

#if !defined(__WIN__)
    static bal_thread t = {0};
    static bal_mutex m  = PTHREAD_MUTEX_INITIALIZER;
#else
    static bal_thread t = INVALID_HANDLE_VALUE;
    static bal_mutex m  = INVALID_HANDLE_VALUE;
#endif

    if (BAL_S_DIE == mask) {
        td.die = 1;

        if (BAL_TRUE == _bal_mutex_destroy(&m)) {
            if (BAL_TRUE == _bal_sdl_clr(&l)) {
                t = NULL;
                r = BAL_TRUE;
            }
        }
    }

    if (s && proc) {
        if (!t) {
            td.die = 0;
            td.sdl = &l;
            td.m   = &m;

            if (BAL_FALSE == _bal_initasyncselect(&t, &m, &td))
                return r;
        }

        if (BAL_TRUE == _bal_mutex_lock(&m)) {
            if (0 == mask) {
                if (BAL_TRUE == _bal_sdl_rem(&l, s->sd))
                    r = BAL_TRUE;
            } else if (_bal_sdl_size(&l) < FD_SETSIZE - 1) {
                bal_selectdata* d = _bal_sdl_find(&l, s->sd);

                if (d) {
                    d->mask = mask;
                    d->proc = proc;
                    r       = BAL_TRUE;
                } else {
                    bal_selectdata d;

                    if (BAL_TRUE == bal_setiomode(s, 1)) {
                        d.mask = mask;
                        d.proc = proc;
                        d.s    = (bal_socket*)s;
                        d._n   = NULL;
                        d._p   = NULL;

                        r = _bal_sdl_add(&l, &d);
                    }
                }
            }

            if (BAL_TRUE == r)
                r = _bal_mutex_unlock(&m);
            else
                (void)_bal_mutex_unlock(&m);
        }
    }

    return r;
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

    if (-1 != (s->sd = socket(af, st, pt))) {
        s->af = af;
        s->pf = pt;
        s->st = st;
        r     = BAL_TRUE;
    }

    return r;
}

int bal_reset(bal_socket* s)
{
    int r = BAL_FALSE;

    if (s) {
        s->af = -1;
        s->pf = -1;
        s->sd = -1;
        s->st = -1;
        r     = BAL_TRUE;
    }

    return r;
}

int bal_close(bal_socket* s)
{
    int r = BAL_FALSE;

    if (s) {
#if defined(__WIN__)
        if (0 == closesocket(s->sd))
            r = bal_reset(s);
#else
        if (0 == close(s->sd))
            r = bal_reset(s);
#endif
        s->_f &= ~BAL_F_PENDCONN;
    }

    return r;
}

int bal_shutdown(bal_socket* s, int how)
{
    if (s) {
        s->_f &= ~BAL_F_PENDCONN;
        return shutdown(s->sd, how);
    } else {
#if defined(__WIN__)
        return SOCKET_ERROR;
#else
        return ENOTSOCK;
#endif
    }
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
                r = connect(s->sd, (const struct sockaddr*)sa, BAL_SASIZE(*sa));

#if defined(__WIN__)
                if (!r || (-1 == r && WSAEWOULDBLOCK == WSAGetLastError())) {
#else
                if (!r || (-1 == r && EWOULDBLOCK == errno)) {
#endif
                    s->_f |= BAL_F_PENDCONN;
                    r = BAL_TRUE;
                    break;
                }
            }
        }
    }

    return r;
}

int bal_send(const bal_socket* s, const void* data, size_t len, int flags)
{
    if (s && data && len)
        return send(s->sd, (const char*)data, len, flags);
    else
        return BAL_FALSE;
}

int bal_recv(const bal_socket* s, void* data, size_t len, int flags)
{
    if (s && data && len)
        return recv(s->sd, (char*)data, len, flags);
    else
        return BAL_FALSE;
}

int bal_sendto(const bal_socket* s, const char* host, const char* port, const void* data, size_t len, int flags)
{
    int r = BAL_FALSE;

    if (s && _bal_validstr(host) && _bal_validstr(port) && data && len) {
        bal_addrinfo ai = {NULL, NULL};

        if (BAL_TRUE == _bal_getaddrinfo(0, PF_UNSPEC, SOCK_DGRAM, host, port, &ai)) {
            r = bal_sendtoaddr(s, (const bal_sockaddr*)ai._ai->ai_addr, data, len, flags);
            freeaddrinfo(ai._ai);
        }
    }

    return r;
}

int bal_sendtoaddr(const bal_socket* s, const bal_sockaddr* sa, const void* data, size_t len, int flags)
{
    if (s && sa && data && len)
        return sendto(s->sd, data, len, flags, (const struct sockaddr*)sa, BAL_SASIZE(*sa));
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

int bal_listen(const bal_socket* s, int backlog)
{
    return ((NULL != s) ? listen(s->sd, backlog) : BAL_FALSE);
}

int bal_accept(const bal_socket* s, bal_socket* res, bal_sockaddr* resaddr)
{
    int r = BAL_FALSE;

    if (s && res && resaddr) {
        socklen_t sasize = sizeof(bal_sockaddr);
        if (-1 != (res->sd = accept(s->sd, (struct sockaddr*)resaddr, &sasize)))
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

int bal_islistening(const bal_socket* s)
{
    int r = BAL_FALSE;
    int l = 0;

    if ((BAL_TRUE == bal_getoption(s, SOL_SOCKET, SO_ACCEPTCONN, &l, sizeof(int))) && 0 != l)
        r = BAL_TRUE;

    return r;
}

int bal_isreadable(const bal_socket* s)
{
    int r = BAL_FALSE;

    if (s) {
        fd_set fd;
        struct timeval t = {0};

        FD_ZERO(&fd);
        FD_SET(s->sd, &fd);

        if (0 == select(1, &fd, NULL, NULL, &t)) {
            if (FD_ISSET(s->sd, &fd))
                r = BAL_TRUE;
        }
    }

    return r;
}

int bal_iswritable(const bal_socket* s)
{
    int r = BAL_FALSE;

    if (s) {
        fd_set fd;
        struct timeval t = {0};

        FD_ZERO(&fd);
        FD_SET(s->sd, &fd);

        if (0 == select(1, NULL, &fd, NULL, &t)) {
            if (FD_ISSET(s->sd, &fd))
                r = BAL_TRUE;
        }
    }

    return r;
}

int bal_setiomode(const bal_socket* s, bool async)
{
#if defined(__WIN__)
    unsigned long flag = async ? 1 : 0;
    return ((NULL != s) ? ioctlsocket(s->sd, FIONBIO, &flag) : BAL_FALSE);
#else
    return ((NULL != s) ? fcntl(s->sd, F_SETFL, async ? O_NONBLOCK : 0) : BAL_FALSE);
#endif
}

size_t bal_recvqueuesize(const bal_socket* s)
{
    size_t r = 0UL;

    if (s) {
#if defined(__WIN__)
        if (0 != ioctlsocket(s->sd, FIONREAD, (void*)&r))
            r = 0ul;
#else
        if (0 != ioctl(s->sd, FIONREAD, &r))
            r = 0ul;
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
    int r = BAL_FALSE;
    bal_sockaddr sa;

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
    int r = BAL_FALSE;
    bal_sockaddr sa;

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
            free(al->_p);
            al->_p = a;
        }

        r      = BAL_TRUE;
        al->_p = al->_a = NULL;
    }

    return r;
}

int bal_getaddrstrings(const bal_sockaddr* in, int dns, bal_addrstrings* out)
{
    int r = BAL_FALSE;

    if (in && out) {
        memset(out, 0, sizeof(bal_addrstrings));

        if (BAL_TRUE == _bal_getnameinfo(BAL_NI_NODNS, in, out->ip, out->port)) {
            if (dns) {
                int get = _bal_getnameinfo(BAL_NI_DNS, in, out->host, out->port);
                if (BAL_FALSE == get) {
                    strncpy(out->host, BAL_AS_UNKNWN, NI_MAXHOST - 1);
                    out->host[NI_MAXHOST - 1] = '\0';
                }
            }

            if (PF_INET == ((struct sockaddr*)in)->sa_family)
                out->type = BAL_AS_IPV4;
            else if (PF_INET6 == ((struct sockaddr*)in)->sa_family)
                out->type = BAL_AS_IPV6;
            else
                out->type = BAL_AS_UNKNWN;

            r = BAL_TRUE;
        }
    }

    return r;
}


/*─────────────────────────────────────────────────────────────────────────────╮
│                            Internal Functions                                │
╰─────────────────────────────────────────────────────────────────────────────*/


int _bal_getaddrinfo(int f, int af, int st, const char* host, const char* port,
    bal_addrinfo* res)
{
    int r = BAL_FALSE;

    if (_bal_validstr(host) && res) {
        if (host) {
            struct addrinfo hints = {0};

            hints.ai_flags    = f;
            hints.ai_family   = af;
            hints.ai_socktype = st;

            r = getaddrinfo(host, port, (const struct addrinfo*)&hints, &res->_ai);

            if (!r)
                res->_p = res->_ai;
            else
                res->_p = res->_ai = NULL;
        }
    }

    if (BAL_TRUE != r && BAL_FALSE != r) {
        _bal_setlasterror(r);
        r = BAL_FALSE;
    }

    return r;
}

int _bal_getnameinfo(int f, const bal_sockaddr* in, char* host, char* port)
{
    int r = BAL_FALSE;

    if (in && host) {
        socklen_t inlen = BAL_SASIZE(*in);
        r = getnameinfo((const struct sockaddr*)in, inlen, host, NI_MAXHOST,
            port, NI_MAXSERV, f);
    }

    if (BAL_TRUE != r && BAL_FALSE != r) {
        _bal_setlasterror(r);
        r = BAL_FALSE;
    }

    return r;
}

const struct addrinfo* _bal_enumaddrinfo(bal_addrinfo* ai)
{
    const struct addrinfo* r = NULL;

    if (ai && ai->_ai) {
        if (ai->_p) {
            r      = ai->_p;
            ai->_p = ai->_p->ai_next;
        } else {
            ai->_p = ai->_ai;
        }
    }

    return r;
}

int _bal_aitoal(bal_addrinfo* in, bal_addrlist* out)
{
    int r = BAL_FALSE;

    if (in && in->_ai && out) {
        const struct addrinfo* ai = NULL;
        bal_addr** a              = &out->_a;
        r                         = BAL_TRUE;

        in->_p = in->_ai;

        while (NULL != (ai = _bal_enumaddrinfo(in))) {
            *a = calloc(1ul, sizeof(bal_addr));

            if (!*a) {
                r = BAL_FALSE;
                break;
            }

            memcpy(&(*a)->_sa, ai->ai_addr, ai->ai_addrlen);
            a = &(*a)->_n;
        }

        bal_resetaddrlist(out);
    }

    return r;
}

int _bal_getlasterror(const bal_socket* s, bal_error* err)
{
    int r = BAL_FALSE;

    if (err) {
        memset(err, 0, sizeof(bal_error));

        if (s) {
            err->code = bal_geterror(s);
        } else {
#if defined(__WIN__)
            err->code = WSAGetLastError();
#else
            err->code = errno;
#endif
        }

#if defined(__WIN__)
        if (0 != FormatMessage(0x00001200U, NULL, err->code, 0U, err->desc,
            BAL_MAXERROR, NULL))
            r = BAL_TRUE;
#else
        if (err->code == EAI_AGAIN  || err->code == EAI_BADFLAGS ||
            err->code == EAI_FAIL   || err->code == EAI_FAMILY   ||
            err->code == EAI_MEMORY || err->code == EAI_NONAME   ||
            err->code == EAI_NODATA || err->code == EAI_SERVICE  ||
            err->code == EAI_SOCKTYPE) {
            if (0 == _bal_retstr(err->desc, gai_strerror(err->code)))
                r = BAL_TRUE;
        } else {
            if (0 == _bal_retstr(err->desc, (const char*)strerror(err->code)))
                r = BAL_TRUE;
        }

#endif
    }

    return r;
}

void _bal_setlasterror(int err)
{
#if defined(__WIN__)
    WSASetLastError(err);
#else
    errno = err;
#endif
}

int _bal_retstr(char* out, const char* in)
{
    int r = BAL_FALSE;

    strncpy(out, in, BAL_MAXERROR - 1);
    out[BAL_MAXERROR - 1] = '\0';
    r = BAL_TRUE;

    return r;
}

int _bal_haspendingconnect(const bal_socket* s)
{
    return (s && ((s->_f & BAL_F_PENDCONN) == BAL_F_PENDCONN))
        ? BAL_TRUE : BAL_FALSE;
}

int _bal_isclosedcircuit(const bal_socket* s)
{
    int r = BAL_FALSE;

    if (s) {
        unsigned char buf = '\0';
        int rcv = recv(s->sd, &buf, sizeof(unsigned char), MSG_PEEK);

        if (0 == rcv)
            r = BAL_TRUE;
        else if (-1 == rcv) {
#if defined(__WIN__)
            int error = WSAGetLastError();
            if (WSAENETDOWN == error     || WSAENOTCONN == error  ||
                WSAEOPNOTSUPP == error   || WSAESHUTDOWN == error ||
                WSAECONNABORTED == error || WSAECONNRESET == error)
                r = BAL_TRUE;
#else
            if (EBADF == errno || ENOTCONN == errno || ENOTSOCK == errno)
                r = BAL_TRUE;
#endif
        }
    }

    return r;
}

BALTHREAD _bal_eventthread(void* p)
{
    bal_eventthread_data* td = (bal_eventthread_data*)p;

    if (td) {
        bal_selectdata* t = NULL;
        fd_set r = {0};
        fd_set w = {0};
        fd_set e = {0};

        while (!td->die) {
            if (BAL_TRUE == _bal_mutex_lock(td->m)) {
                int size = _bal_sdl_size(td->sdl);

                if (0 < size) {
                    struct timeval tv = {0, 0};
                    int ret           = -1;
                    bal_descriptor highsd = (bal_descriptor)-1;

                    FD_ZERO(&r);
                    FD_ZERO(&w);
                    FD_ZERO(&e);

                    _bal_sdl_reset(td->sdl);

                    while (BAL_TRUE == _bal_sdl_enum(td->sdl, &t)) {
                        if (BAL_TRUE == _bal_haspendingconnect(t->s)) {
                            t->mask |= BAL_S_CONNECT;
                            t->s->_f &= ~BAL_F_PENDCONN;
                        }

                        if (t->s->sd > highsd)
                            highsd = t->s->sd;

                        FD_SET(t->s->sd, &r);
                        FD_SET(t->s->sd, &w);
                        FD_SET(t->s->sd, &e);
                    }

                    ret = select(highsd + 1, &r, &w, &e, &tv);

                    if (-1 != ret) {
                        _bal_dispatchevents(&r, td, BAL_S_READ);
                        _bal_dispatchevents(&w, td, BAL_S_WRITE);
                        _bal_dispatchevents(&e, td, BAL_S_EXCEPT);
                    }
                }

                _bal_mutex_unlock(td->m);
            }

            FD_ZERO(&r);
            FD_ZERO(&w);
            FD_ZERO(&e);

#if defined(__WIN__)
            Sleep(1);
#else
            sched_yield();
#endif
        }
    } else {
#if defined(__WIN__)
        return 1U;
#else
        return (void*)(1);
#endif
    }

#if defined(__WIN__)
    return 0U;
#else
    return NULL;
#endif
}

int _bal_initasyncselect(bal_thread* t, bal_mutex* m, bal_eventthread_data* td)
{
    int r = BAL_FALSE;

    if (t && m && td) {
        td->die = 0;

        if (BAL_TRUE == _bal_mutex_init(m)) {
#if defined(__WIN__)
            if (NULL != (*t = (bal_thread)_beginthreadex(NULL, 0U,
                _bal_eventthread, td, 0U, NULL)))
                r = BAL_TRUE;
#else
            if (0 == pthread_create(t, NULL, _bal_eventthread, td))
                r = BAL_TRUE;
#endif
        }
    }

    return r;
}

void _bal_dispatchevents(fd_set* set, bal_eventthread_data* td, int type)
{
    if (set && td) {
        bal_selectdata* t = NULL;

        _bal_sdl_reset(td->sdl);

        while (BAL_TRUE == _bal_sdl_enum(td->sdl, &t)) {
            if (0 != FD_ISSET(t->s->sd, set)) {
                int event = 0;
                int snd   = 0;

                switch (type) {
                    case BAL_S_READ:
                        if (t->mask & BAL_E_READ) {
                            if (BAL_TRUE == _bal_isclosedcircuit(t->s))
                                event = BAL_E_CLOSE;
                            else if (BAL_TRUE == bal_islistening(t->s))
                                event = BAL_E_ACCEPT;
                            else
                                event = BAL_E_READ;

                            snd = 1;
                        }
                    break;
                    case BAL_S_WRITE:
                        if (t->mask & BAL_E_WRITE) {
                            if (t->mask & BAL_S_CONNECT) {
                                event = BAL_E_CONNECT;
                                t->mask &= ~BAL_S_CONNECT;
                            } else {
                                event = BAL_E_WRITE;
                            }

                            snd = 1;
                        }
                    break;
                    case BAL_S_EXCEPT:
                        if (t->mask & BAL_S_CONNECT) {
                            event = BAL_E_CONNFAIL;
                            t->mask &= ~BAL_S_CONNECT;
                        } else {
                            event = BAL_E_EXCEPTION;
                        }

                        snd = 1;
                    break;
                    default:
                        _bal_setlasterror(EINVAL);
                    break;
                }

                if (1 == snd)
                    t->proc(t->s, event);

                if (BAL_E_CLOSE == event)
                    _bal_sdl_rem(td->sdl, t->s->sd);
            }
        }
    }
}

int _bal_sdl_add(bal_selectdata_list* sdl, const bal_selectdata* d)
{
    int r = BAL_FALSE;

    if (sdl && d) {
        bal_selectdata** t = NULL;
        bal_selectdata* n  = NULL;

        if (sdl->_h) {
            t = &sdl->_t->_n;
            n = sdl->_t;
        } else {
            t = &sdl->_h;
        }

        *t = malloc(sizeof(bal_selectdata));

        if (*t) {
            memcpy(*t, d, sizeof(bal_selectdata));

            (*t)->_n = NULL;
            (*t)->_p = n;
            sdl->_t  = *t;
            r        = BAL_TRUE;
        }
    }

    return r;
}

int _bal_sdl_rem(bal_selectdata_list* sdl, bal_descriptor sd)
{
    int r = BAL_FALSE;

    if (sdl && sd != BAL_BADSOCKET) {
        bal_selectdata* t = _bal_sdl_find(sdl, sd);

        if (t) {
            if (t == sdl->_h) {
                sdl->_h = t->_n;
            } else {
                t->_p->_n = t->_n;

                if (t == sdl->_t)
                    sdl->_t = t->_p;
                else
                    t->_n->_p = t->_p;
            }

            free(t);
            r = BAL_TRUE;
        }
    }

    return r;
}

int _bal_sdl_clr(bal_selectdata_list* sdl)
{
    int r = BAL_FALSE;

    if (sdl) {
        if (0 == _bal_sdl_size(sdl))
            r = BAL_TRUE;
        else if (sdl->_h) {
            bal_selectdata* t  = sdl->_h;
            bal_selectdata* t2 = NULL;

            while (t) {
                t2 = t->_n;
                free(t);
                t = t2;
            }

            sdl->_h = sdl->_t = sdl->_c = NULL;
            r                           = BAL_TRUE;
        }
    }

    return r;
}

int _bal_sdl_size(bal_selectdata_list* sdl)
{
    int r = 0;

    if (sdl) {
        bal_selectdata* d = sdl->_h;

        while (d) {
            d = d->_n;
            r++;
        }
    }

    return r;
}

int _bal_sdl_copy(bal_selectdata_list* dest, bal_selectdata_list* src)
{
    int r = BAL_FALSE;

    if (dest && src) {
        bal_selectdata* d = src->_h;
        int copied        = 0;

        _bal_sdl_clr(dest);
        _bal_sdl_reset(src);

        while (d) {
            if (BAL_TRUE == _bal_sdl_add(dest, d))
                copied++;
            d = d->_n;
        }

        if (0 < copied)
            r = BAL_TRUE;
    }

    return r;
}

int _bal_sdl_enum(bal_selectdata_list* sdl, bal_selectdata** d)
{
    int r = BAL_FALSE;

    if (sdl && d) {
        if (sdl->_c) {
            *d      = sdl->_c;
            sdl->_c = sdl->_c->_n;
            r       = BAL_TRUE;
        } else {
            _bal_sdl_reset(sdl);
        }
    }

    return r;
}

void _bal_sdl_reset(bal_selectdata_list* sdl)
{
    if (sdl)
        sdl->_c = sdl->_h;
}

bal_selectdata* _bal_sdl_find(const bal_selectdata_list* sdl, bal_descriptor sd)
{
    bal_selectdata* r = NULL;

    if (sdl && sd != BAL_BADSOCKET) {
        bal_selectdata* t = sdl->_h;

        while (t) {
            if (t->s->sd == sd) {
                r = t;
                break;
            }

            t = t->_n;
        }
    }

    return r;
}

int _bal_mutex_init(bal_mutex* m)
{
    int r = BAL_FALSE;

    if (m) {
#if defined(__WIN__)
        *m = CreateMutex(NULL, FALSE, NULL);
        if (*m)
            r = BAL_TRUE;
#else
        if (0 == pthread_mutex_init(m, NULL))
            r = BAL_TRUE;
#endif
    }

    return r;
}

int _bal_mutex_lock(bal_mutex* m)
{
    int r = BAL_FALSE;

    if (m) {
#if defined(__WIN__)
        if (WAIT_OBJECT_0 == WaitForSingleObject(*m, INFINITE))
            r = BAL_TRUE;
#else
        if (0 == pthread_mutex_lock(m))
            r = BAL_TRUE;
#endif
    }

    return r;
}

int _bal_mutex_unlock(bal_mutex* m)
{
    int r = BAL_FALSE;

    if (m) {
#if defined(__WIN__)
        if (ReleaseMutex(*m))
            r = BAL_TRUE;
#else
        if (0 == pthread_mutex_unlock(m))
            r = BAL_TRUE;
#endif
    }

    return r;
}

int _bal_mutex_destroy(bal_mutex* m)
{
    int r = BAL_FALSE;

    if (m) {
#if defined(__WIN__)
        if (*m && INVALID_HANDLE_VALUE != *m) {
            if (CloseHandle(*m))
                r = BAL_TRUE;
            *m = NULL;
        }
#else
        if (0 == pthread_mutex_destroy(m))
            r = BAL_TRUE;
#endif
    }

    return r;
}
