/*
 *  sal.c : ANSI C implementation of SAL (Socket Abstraction Library)
 *
 *  Copyright 2004 - All Rights Reserved
 *
 *  Description:
 *    This file contains all of the ANSI C functions that comprise the
 *    SAL library.  All functions return error / success codes compliant
 *    with the socket implementation for the system.
 *
 *  Author:
 *    Ryan Matthew Lederman
 *    ryan@ript.net
 *
 *  Date:
 *    August 08, 2004
 *
 *  Version: 1.0
 *
 *  License:
 *    This software is provided to the end user "as-is", with no warranty
 *    implied or otherwise.  The author is for all intents and purposes
 *    the owner of this source code.  The author grants the end user the
 *    privilege to use this software in any application, commercial or
 *    otherwise, with the following restrictions:
 *      1.) If the end user wishes to modify this source code, he/she must
 *       not distribute the source code as the original source code, and
 *       must clearly document the changes made to the source code in any
 *       distribution of said source code.
 *      2.) This license agreement must always be displayed in any version
 *       of the source code, modified or otherwise.
 *    The author will not be held liable for ANY damage, loss of data,
 *    loss of income, or any other form of loss that results directly or 
 *    indirectly from the use of this software.
 */
#include "sal.h"

/*
  Exported functions
 */

int Sal_Initialize(void) {

  int r = SAL_FALSE;

#if defined(_WIN32)
{
  WORD wVer   = MAKEWORD(WSOCK_MINVER, WSOCK_MAJVER);
  WSADATA wd  = {0};

  if (0 == WSAStartup(wVer, &wd)) {
    r = SAL_TRUE;
  }
}
#else
  r = SAL_TRUE;
#endif /* !_WIN32 */

  return r;
}

int Sal_Finalize(void) {

  int r = SAL_FALSE;

#if defined(_WIN32)
{
  if (0 == WSACleanup()) {
    r = Sal_AsyncSelect(NULL, NULL, SAL_S_DIE);
  }
}
#else
  r = Sal_AsyncSelect(NULL, NULL, SAL_S_DIE);
#endif

  return r;
}

int Sal_AsyncSelect(const sal_t *s, SALEVENTPROC proc, unsigned long mask) {
  
  int r                           = SAL_FALSE;
  static sal_selectdata_list l    = {NULL};
  static sal_eventthread_data td  = {NULL};
  static sal_thread_t t           = NULL;
  static sal_mutex_t m            = {NULL};

  if (SAL_S_DIE == mask) {
    
    td.die = 1;

    if (SAL_TRUE == _Sal_Mutex_Free(&m)) {

      if (SAL_TRUE == _Sal_SDL_Clr(&l)) {

        t = NULL;
        memset(&m, 0, sizeof(sal_mutex_t));
        r = SAL_TRUE;

      }

    }

  }

  if ((s) && (proc)) {

    if (!t) {

      td.die = 0;
      td.l   = &l;
      td.m   = &m;

      if (SAL_FALSE == _Sal_InitAsyncSelect(&t, &m, &td)) {
        return r;
      }

    }

    if (SAL_TRUE == _Sal_Mutex_Lock(&m)) {

      if (0 == mask) {
        
        if (SAL_TRUE == _Sal_SDL_Rem(&l, s->sd)) {
          r = SAL_TRUE;
        }

      } else {

        if (_Sal_SDL_Size(&l) < (FD_SETSIZE - 1)) {

          sal_selectdata *d = _Sal_SDL_Find(&l, s->sd);

          if (d) {
            
            d->mask = mask;
            d->proc = proc;
            r       = SAL_TRUE;

          } else {

            sal_selectdata d;

            if (SAL_TRUE == Sal_SetIOMode(s, 1)) {

              d.mask  = mask;
              d.proc  = proc;
              d.s     = (sal_t *)s;
              d._n    = NULL;
              d._p    = NULL;
            
              r = _Sal_SDL_Add(&l, &d);

            }

          }

        }

      }

      if (SAL_TRUE == r) {
        r = _Sal_Mutex_Unlock(&m);
      } else {
        _Sal_Mutex_Unlock(&m);
      }

    }

  }

  return r;
}

int Sal_AutoSocket(sal_t *s, int af, int pt, ctstr_t host, ctstr_t port) {

  int r = SAL_FALSE;

  if ((s) && _SETSTR(host)) {

    int _af = ((af == 0) ? PF_UNSPEC : af);
    int _st = ((pt == 0) ? 0 : (pt == IPPROTO_TCP) ? SOCK_STREAM : SOCK_DGRAM);
    sal_addrinfo ai;
    
    memset(&ai, 0, sizeof(ai));

    if (SAL_TRUE == _Sal_GetAddrInfo(0, _af, _st, host, port, &ai)) {

      const struct addrinfo *a = NULL;

      while (NULL != (a = _Sal_EnumAddrInfo(&ai))) {

        if (SAL_TRUE == Sal_Socket(s, a->ai_family, a->ai_protocol, a->ai_socktype)) {
          r = SAL_TRUE;
          break;
        }

      }

      freeaddrinfo(ai._ai);
    }

  }

  return r;
}

int Sal_Socket(sal_t *s, int af, int pt, int st) {

  int r = SAL_FALSE;

  if (-1L != (s->sd = socket(af, st, pt))) {

    s->af = af;
    s->pf = pt;
    s->st = st;
    r     = SAL_TRUE;

  }

  return r;
}

int Sal_Reset(sal_t *s) {

  int r = SAL_FALSE;

  if (s) {

    s->af = -1;
    s->pf = -1;
    s->sd = ((sd_t)-1);
    s->st = -1;
    s->ud = ((unsigned long)-1);
    r     = SAL_TRUE;

  }

  return r;
}

int Sal_Close(sal_t *s) {

  int r = SAL_FALSE;

  if (s) {
#if defined(_WIN32)
    if (0 == closesocket(s->sd)) {
      r = Sal_Reset(s);
    }
#else
    if (0 == close(s->sd)) {
      r = Sal_Reset(s);
    }
#endif
  }

  s->_f &= ~SAL_F_PENDCONN;

  return r;
}

int Sal_ShutDown(sal_t *s, int how) {
  
  if ((NULL != s)) {

    s->_f &= ~SAL_F_PENDCONN;
    return shutdown(s->sd, how);

  } else {

#if defined(_WIN32)
    return SOCKET_ERROR;
#else
    return ENOTSOCK;
#endif // !_WIN32
  }

}

int Sal_Connect(const sal_t *s, ctstr_t host, ctstr_t port) {

  int r = SAL_FALSE;

  if ((s) && _SETSTR(host) && _SETSTR(port)) {

    sal_addrinfo ai;

    memset(&ai, 0L, sizeof(ai));

    if (SAL_TRUE == _Sal_GetAddrInfo(0, s->af, s->st, host, port, &ai)) {

      sal_addrlist al;

      memset(&al, 0, sizeof(al));

      if (SAL_TRUE == _Sal_AIToAL(&ai, &al)) {

        r = Sal_ConnectAddrList((sal_t *)s, &al);

        Sal_FreeAddrList(&al);
      }

      freeaddrinfo(ai._ai);
    }

  }

  return r;
}

int Sal_ConnectAddrList(sal_t *s, sal_addrlist *al) {

  int r = SAL_FALSE;

  if (s) {

    if (SAL_TRUE == Sal_ResetAddrList(al)) {

      const sal_sockaddr *sa = NULL;

      while (NULL != (sa = Sal_EnumAddrList(al))) {

        r = connect(s->sd, (const struct sockaddr *)sa, SAL_SASIZE((*sa)));

#if defined(_WIN32)

        if ((!r) || ((-1 == r) && (WSAEWOULDBLOCK == WSAGetLastError()))) {
#else
        if ((!r) || ((-1 == r) && (EWOULDBLOCK == r))) {

#endif /* !_WIN32 */

          s->_f |= SAL_F_PENDCONN;
          r = SAL_TRUE;
          break;

        }

      }

    }

  }

  return r;
}

int Sal_Send(const sal_t *s, const void *data, size_t len, int flags) {

  if ((s) && (data) && (len)) {
    return send(s->sd, (const char *)data, len, flags);
  } else {
    return SAL_FALSE;
  }

}

int Sal_Recv(const sal_t *s, void *data, size_t len, int flags) {

  if ((s) && (data) && (len)) {
    return recv(s->sd, (char *)data, len, flags);
  } else {
    return SAL_FALSE;
  }

}

int Sal_SendTo(const sal_t *s, ctstr_t host, ctstr_t port, const void *data, size_t len, int flags) {

  int r = SAL_FALSE;

  if ((s) && _SETSTR(host) && _SETSTR(port) && (data) && (len)) {

    sal_addrinfo ai;

    memset(&ai, 0, sizeof(ai));

    if (SAL_TRUE == _Sal_GetAddrInfo(0, PF_UNSPEC, SOCK_DGRAM, host, port, &ai)) {

      r = Sal_SendToAddr(s, (const sal_sockaddr *)ai._ai->ai_addr, data, 
                         len, flags);

      freeaddrinfo(ai._ai);
    }

  }

  return r;
}

int Sal_SendToAddr(const sal_t *s, const sal_sockaddr *sa, const void *data, size_t len, int flags) {

  if ((s) && (sa) && (data) && (len)) {
    return sendto(s->sd, data, len, flags, (const struct sockaddr *)sa, 
                  SAL_SASIZE((*sa)));
  } else {
    return SAL_FALSE;
  }

}

int Sal_RecvFrom(const sal_t *s, void *data, size_t len, int flags, sal_sockaddr *res) {

  if ((s) && (data) && (len)) {
    
    socklen_t sasize = sizeof(sal_sockaddr);

    return recvfrom(s->sd, (char *)data, len, flags, (struct sockaddr *)res, &sasize);

  } else {
    return SAL_FALSE;
  }

}

int Sal_Bind(const sal_t *s, ctstr_t addr, ctstr_t port) {

  int r = SAL_FALSE;

  if ((s) && _SETSTR(addr) && _SETSTR(port)) {

    sal_addrinfo ai;

    memset(&ai, 0, sizeof(ai));

    if (SAL_TRUE == _Sal_GetAddrInfo(AI_NUMERICHOST, s->af, s->st, addr, port, &ai)) {

      const struct addrinfo *a = NULL;

      while (NULL != (a = _Sal_EnumAddrInfo(&ai))) {

        if (0L == bind(s->sd, (const struct sockaddr *)a->ai_addr, a->ai_addrlen)) {
          r = SAL_TRUE;
          break;
        }

      }

      freeaddrinfo(ai._ai);
    }

  }

  return r;
}

int Sal_Listen(const sal_t *s, int backlog) {
  return ((NULL != s) ? listen(s->sd, backlog) : SAL_FALSE);
}

int Sal_Accept(const sal_t *s, sal_t *res, sal_sockaddr *resaddr) {

  int r = SAL_FALSE;

  if ((s) && (res) && (resaddr)) {

    socklen_t sasize = sizeof(sal_sockaddr);

    if (-1L != (res->sd = accept(s->sd, (struct sockaddr *)resaddr, &sasize))) {
      r = SAL_TRUE;
    }

  }

  return r;
}

int Sal_GetOption(const sal_t *s, int level, int name, void *optval, socklen_t len) {
  return ((NULL != s) ? getsockopt(s->sd, level, name, optval, &len) : SAL_FALSE);
}

int Sal_SetOption(const sal_t *s, int level, int name, const void *optval, socklen_t len) {
  return ((NULL != s) ? setsockopt(s->sd, level, name, optval, len) : SAL_FALSE);
}

int Sal_SetBroadcast(const sal_t *s, int flag) {
  return Sal_SetOption(s, SOL_SOCKET, SO_BROADCAST, &flag, sizeof(int));
}

int Sal_GetBroadcast(const sal_t *s) {

  int r = SAL_FALSE;

  if (SAL_FALSE == Sal_GetOption(s, SOL_SOCKET, SO_BROADCAST, &r, sizeof(int))) {
    r = SAL_FALSE;
  }

  return r;
}

int Sal_SetDebug(const sal_t *s, int flag) {
  return Sal_SetOption(s, SOL_SOCKET, SO_DEBUG, &flag, sizeof(int));
}

int Sal_GetDebug(const sal_t *s) {

  int r = SAL_FALSE;

  if (SAL_FALSE == Sal_GetOption(s, SOL_SOCKET, SO_DEBUG, &r, sizeof(int))) {
    r = SAL_FALSE;
  }

  return r;
}

int Sal_SetLinger(const sal_t *s, int sec) {

  struct linger l;

  l.l_onoff   = (0 != sec) ? 1 : 0;
  l.l_linger  = sec;

  return Sal_SetOption(s, SOL_SOCKET, SO_LINGER, &l, sizeof(l));
}

int Sal_GetLinger(const sal_t *s, int *sec) {

  int r = SAL_FALSE;

  if (sec) {

    struct linger l;

    memset(&l, 0, sizeof(l));

    if (SAL_TRUE == Sal_GetOption(s, SOL_SOCKET, SO_LINGER, &l, sizeof(l))) {
      *sec  = l.l_linger;
      r     = SAL_TRUE;
    }

  }

  return r;
}

int Sal_SetKeepAlive(const sal_t *s, int flag) {
  return Sal_SetOption(s, SOL_SOCKET, SO_KEEPALIVE, &flag, sizeof(int));
}

int Sal_GetKeepAlive(const sal_t *s) {

  int r = SAL_FALSE;

  if (SAL_FALSE == Sal_GetOption(s, SOL_SOCKET, SO_KEEPALIVE, &r, sizeof(int))) {
    r = SAL_FALSE;
  }

  return r;
}

int Sal_SetOOBInline(const sal_t *s, int flag) {
  return Sal_SetOption(s, SOL_SOCKET, SO_OOBINLINE, &flag, sizeof(int));
}

int Sal_GetOOBInline(const sal_t *s) {

  int r = SAL_FALSE;

  if (SAL_FALSE == Sal_GetOption(s, SOL_SOCKET, SO_OOBINLINE, &r, sizeof(int))) {
    r = SAL_FALSE;
  }

  return r;
}

int Sal_SetReuseAddr(const sal_t *s, int flag) {
  return Sal_SetOption(s, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(int));
}

int Sal_GetReuseAddr(const sal_t *s) {

  int r = SAL_FALSE;

  if (SAL_FALSE == Sal_GetOption(s, SOL_SOCKET, SO_REUSEADDR, &r, sizeof(int))) {
    r = SAL_FALSE;
  }

  return r;
}

int Sal_SetSendBufSize(const sal_t *s, int size) {
  return Sal_SetOption(s, SOL_SOCKET, SO_SNDBUF, &size, sizeof(int));
}

int Sal_GetSendBufSize(const sal_t *s) {

  int r = SAL_FALSE;

  if (SAL_FALSE == Sal_GetOption(s, SOL_SOCKET, SO_SNDBUF, &r, sizeof(int))) {
    r = SAL_FALSE;
  }

  return r;
}

int Sal_SetRecvBufSize(const sal_t *s, int size) {
  return Sal_SetOption(s, SOL_SOCKET, SO_RCVBUF, &size, sizeof(int));
}

int Sal_GetRecvBufSize(const sal_t *s) {

  int r = SAL_FALSE;

  if (SAL_FALSE == Sal_GetOption(s, SOL_SOCKET, SO_RCVBUF, &r, sizeof(int))) {
    r = SAL_FALSE;
  }

  return r;
}

int Sal_SetSendTimeout(const sal_t *s, long sec, long msec) {

  struct timeval t;

  t.tv_sec  = sec;
  t.tv_usec = msec;

  return Sal_SetOption(s, SOL_SOCKET, SO_SNDTIMEO, &t, sizeof(t));
}

int Sal_GetSendTimeout(const sal_t *s, long *sec, long *msec) {

  int r = SAL_FALSE;

  if ((sec) && (msec)) {

    struct timeval t;

    memset(&t, 0, sizeof(t));

    if (SAL_TRUE == Sal_GetOption(s, SOL_SOCKET, SO_SNDTIMEO, &t, sizeof(t))) {
      *sec  = t.tv_sec;
      *msec = t.tv_usec;
      r     = SAL_TRUE;
    }

  }

  return r;
}

int Sal_SetRecvTimeout(const sal_t *s, long sec, long msec) {

  struct timeval t;

  t.tv_sec  = sec;
  t.tv_usec = msec;

  return Sal_SetOption(s, SOL_SOCKET, SO_RCVTIMEO, &t, sizeof(t));
}

int Sal_GetRecvTimeout(const sal_t *s, long *sec, long *msec) {

  int r = SAL_FALSE;

  if ((sec) && (msec)) {

    struct timeval t;

    memset(&t, 0, sizeof(t));

    if (SAL_TRUE == Sal_GetOption(s, SOL_SOCKET, SO_RCVTIMEO, &t, sizeof(t))) {
      *sec  = t.tv_sec;
      *msec = t.tv_usec;
      r     = SAL_TRUE;
    }

  }

  return r;
}

int Sal_GetError(const sal_t *s) {

  int r = SAL_FALSE;

  if (SAL_FALSE == Sal_GetOption(s, SOL_SOCKET, SO_ERROR, &r, sizeof(int))) {
    r = SAL_FALSE;
  }

  return r;
}

int Sal_IsListening(const sal_t *s) {
  
  int r = SAL_FALSE;
  int l = 0;

  if ((SAL_TRUE == Sal_GetOption(s, SOL_SOCKET, SO_ACCEPTCONN, &l, sizeof(int))) && (l)) {
    r = SAL_TRUE;
  }

  return r;
}

int Sal_IsReadable(const sal_t *s) {

  int r = SAL_FALSE;

  if (s) {

    fd_set fd;
    struct timeval t;

    FD_ZERO(&fd);
    FD_SET(s->sd, &fd);

    t.tv_sec  = 0L;
    t.tv_usec = 0L;

    if (0 == select(1, &fd, NULL, NULL, &t)) {
  
      if (FD_ISSET(s->sd, &fd)) {
        r = SAL_TRUE;
      }

    }

  }

  return r;
}

int Sal_IsWritable(const sal_t *s) {

  int r = SAL_FALSE;

  if (s) {

    fd_set fd;
    struct timeval t;

    FD_ZERO(&fd);
    FD_SET(s->sd, &fd);

    t.tv_sec  = 0L;
    t.tv_usec = 0L;

    if (0 == select(1, NULL, &fd, NULL, &t)) {
  
      if (FD_ISSET(s->sd, &fd)) {
        r = SAL_TRUE;
      }

    }

  }

  return r;
}

int Sal_SetIOMode(const sal_t *s, unsigned long flag) {

#if defined(_WIN32)
  return ((NULL != s) ? ioctlsocket(s->sd, FIONBIO, &flag) : SAL_FALSE);
#else
  return ((NULL != s) ? ioctl(s->sd, FIONBIO, &flag) : SAL_FALSE);
#endif
  
}

size_t Sal_RecvQueueSize(const sal_t *s) {

  size_t r = 0UL;

  if (s) {
    
#if defined(_WIN32)
    if (0 != ioctlsocket(s->sd, FIONREAD, (void *)&r)) {
      r = 0U;
    }
#else
    if (0 != ioctl(s->sd, FIONREAD, &r)) {
      r = 0U;
    }
#endif
  }

  return r;
}

int Sal_LastLibError(sal_error *err) {
  return _Sal_GetLastError(NULL, err);
}

int Sal_LastSockError(const sal_t *s, sal_error *err) {
  return _Sal_GetLastError(s, err);
}

int Sal_ResolveHost(ctstr_t host, sal_addrlist *out) {

  int r = SAL_FALSE;

  if (_SETSTR(host) && (out)) {

    sal_addrinfo ai;

    memset(&ai, 0, sizeof(ai));

    if (SAL_TRUE == _Sal_GetAddrInfo(0, PF_UNSPEC, SOCK_STREAM, 
                                     host, NULL, &ai))
    {
      if (SAL_TRUE == _Sal_AIToAL(&ai, out)) {
        r = SAL_TRUE;
      }

      freeaddrinfo(ai._ai);
    }

  }

  return r;
}

int Sal_GetRemoteHostAddr(const sal_t *s, sal_sockaddr *out) {

  int r = SAL_FALSE;

  if ((s) && (out)) {

    socklen_t salen = sizeof(sal_sockaddr);

    memset(out, 0, sizeof(sal_sockaddr));

    if (0 == getpeername(s->sd, (struct sockaddr *)out, &salen)) {
      r = SAL_TRUE;
    }

  }

  return r;
}

int Sal_GetRemoteHostStrings(const sal_t *s, int dns, sal_addrstrings *out) {

  int r = SAL_FALSE;
  sal_sockaddr sa;

  if (SAL_TRUE == Sal_GetRemoteHostAddr(s, &sa)) {
    r = Sal_GetAddrStrings(&sa, dns, out);
  }

  return r;
}

int Sal_GetLocalHostAddr(const sal_t *s, sal_sockaddr *out) {

  int r = SAL_FALSE;

  if ((s) && (out)) {

    socklen_t salen = sizeof(sal_sockaddr);

    memset(out, 0, sizeof(sal_sockaddr));

    if (0 == getsockname(s->sd, (struct sockaddr *)out, &salen)) {
      r = SAL_TRUE;
    }

  }

  return r;
}

int Sal_GetLocalHostStrings(const sal_t *s, int dns, sal_addrstrings *out) {

  int r = SAL_FALSE;
  sal_sockaddr sa;

  if (SAL_TRUE == Sal_GetLocalHostAddr(s, &sa)) {
    r = Sal_GetAddrStrings(&sa, dns, out);
  }

  return r;
}

int Sal_ResetAddrList(sal_addrlist *al) {

  int r = SAL_FALSE;

  if (al) {
    al->_p = al->_a;
    r      = SAL_TRUE;
  }

  return r;
}

const sal_sockaddr *Sal_EnumAddrList(sal_addrlist *al) {

  const sal_sockaddr *r = NULL;

  if (al) {
    if (al->_a) {
      if (al->_p) {
        r       = &al->_p->_sa;
        al->_p  = al->_p->_n;
      } else {
        Sal_ResetAddrList(al);
      }
    }
  }

  return r;
}

int Sal_FreeAddrList(sal_addrlist *al) {

  int r = SAL_FALSE;

  if (al) {
    
    sal_addr *a = al->_p = al->_a;

    while (al->_p) {

      a = al->_p->_n;
      free(al->_p);
      al->_p = a;

    }

    r       = SAL_TRUE;
    al->_p  = al->_a = NULL;

  }

  return r;
}

int Sal_GetAddrStrings(const sal_sockaddr *in, int dns, sal_addrstrings *out) {

  int r = SAL_FALSE;

  if ((in) && (out)) {

    memset(out, 0, sizeof(sal_addrstrings));

    if (SAL_TRUE == _Sal_GetNameInfo(SAL_NI_NODNS, in, out->ip, out->port)) {

      if (dns) {

        if (SAL_FALSE == _Sal_GetNameInfo(SAL_NI_DNS, in, out->host, out->port)) {

#ifdef SALWIDECHARS
          wcscpy(out->host, SAL_AS_UNKNWN);
#else
          strcpy(out->host, SAL_AS_UNKNWN);
#endif
        }
        
      }

      if (PF_INET == ((struct sockaddr *)in)->sa_family) {
        out->type = SAL_AS_IPV4;
      } else if (PF_INET6 == ((struct sockaddr *)in)->sa_family) {
        out->type = SAL_AS_IPV6;
      } else {
        out->type = SAL_AS_UNKNWN;
      }

      r = SAL_TRUE;
    }

  }

  return r;
}


/*
  Internal functions
 */


int _Sal_GetAddrInfo(int f, int af, int st, ctstr_t host, ctstr_t port, 
                     sal_addrinfo *res)
{
  int r = SAL_FALSE;

  if (_SETSTR(host) && (res)) {

    const char *mbhost = _Sal_GetMBStr(host);
    const char *mbport = _Sal_GetMBStr(port);

    if (mbhost) {

      struct addrinfo hints;

      memset(&hints, 0, sizeof(hints));

      hints.ai_flags     = f;
      hints.ai_family    = af;
      hints.ai_socktype  = st;

      r = getaddrinfo(mbhost, mbport, (const struct addrinfo *)&hints, &res->_ai);

      if (!r) {
        res->_p = res->_ai;
      } else {
        res->_p = res->_ai = NULL;
      }

#ifdef SALWIDECHARS
      free((void *)mbhost);

      if (mbport) {
        free((void *)mbport);
      }
#endif

    }

  }

  if (SAL_TRUE != r) {
    if (SAL_FALSE != r) {
      _Sal_SetLastError(r);
      r = SAL_FALSE;
    }
  }

  return r;
}

int _Sal_GetNameInfo(int f, const sal_sockaddr *in, tstr_t host, tstr_t port) {

  int r = SAL_FALSE;

  if ((in) && (host)) {

#ifdef SALWIDECHARS
    char mbhost[NI_MAXHOST] = {0};
    char mbport[NI_MAXSERV] = {0};
#else
    char *mbhost = host;
    char *mbport = port;
#endif

    socklen_t inlen = SAL_SASIZE((*in));

    r = getnameinfo((const struct sockaddr *)in, inlen, mbhost, NI_MAXHOST, 
                    mbport, NI_MAXSERV, f);

#ifdef SALWIDECHARS

    if (!r) {
      
      r = SAL_FALSE;

      if (0 == _Sal_RetStr(host, (const char *)mbhost)) {
        if (port) {
          if (0 == _Sal_RetStr(port, (const char *)mbport)) {
            r = SAL_TRUE;
          }
        } else {
          r = SAL_TRUE;
        }
      }

    }

#endif

  }

  if (SAL_TRUE != r) {
    if (SAL_FALSE != r) {
      _Sal_SetLastError(r);
      r = SAL_FALSE;
    }
  }

  return r;
}

const struct addrinfo *_Sal_EnumAddrInfo(sal_addrinfo *ai) {

  const struct addrinfo *r = NULL;

  if (ai) {
    if (ai->_ai) {
      if (ai->_p) {
        r       = ai->_p;
        ai->_p  = ai->_p->ai_next;
      } else {
        ai->_p  = ai->_ai;
      }
    }
  }

  return r;
}

int _Sal_AIToAL(sal_addrinfo *in, sal_addrlist *out) {

  int r = SAL_FALSE;

  if ((in) && (in->_ai) && (out)) {

    const struct addrinfo *ai = NULL;
    sal_addr **a              = &out->_a;
    r                         = SAL_TRUE;

    in->_p = in->_ai;

    while (NULL != (ai = _Sal_EnumAddrInfo(in))) {

      *a = (sal_addr *)calloc(1UL, sizeof(sal_addr));

      if (!(*a)) {
        r = SAL_FALSE;
        break;
      }

      memcpy(&((*a)->_sa), ai->ai_addr, ai->ai_addrlen);

      a = &((*a)->_n);

    }

    Sal_ResetAddrList(out);
  }

  return r;
}

int _Sal_GetLastError(const sal_t *s, sal_error *err) {

  int r = SAL_FALSE;

  if (err) {
    
    memset(err, 0L, sizeof(sal_error));

    if (s) {

      err->code = Sal_GetError(s);

    } else {

#if defined(_WIN32)
      err->code = WSAGetLastError();
#else
      err->code = errno;
#endif

    }

#if defined(_WIN32)

      if (0U != FormatMessage(0x00001200U, NULL, err->code, 0U, 
                              err->desc, SAL_MAXERRSTR, NULL))
      {
        r = SAL_TRUE;
      }
#else
      
      if (err->code == EAI_AGAIN  || err->code == EAI_BADFLAGS ||
          err->code == EAI_FAIL   || err->code == EAI_FAMILY   ||
          err->code == EAI_MEMORY || err->code == EAI_NONAME   ||
          err->code == EAI_NODATA || err->code == EAI_SERVICE  ||
          err->code == EAI_SOCKTYPE)
      {
        if (0 == _Sal_RetStr(err->desc, gai_strerror(err->code))) {
          r = SAL_TRUE;
        }

      } else {

        if (0 == _Sal_RetStr(err->desc, (const char *)strerror(err->code))) {
          r = SAL_TRUE;
        }

      }

#endif /* !_WIN32 */

  }

  return r;
}

void _Sal_SetLastError(int err) {

#if defined(_WIN32)
  WSASetLastError(err);
#else
  errno = err;
#endif

}

const char *_Sal_GetMBStr(ctstr_t input) {

#ifdef SALWIDECHARS

  char *mbstr   = (char *)calloc(sizeof(char), (wcslen(input) + sizeof(char)));

  if (!mbstr) {
    return (char *)(NULL);
  } else {
    if (((size_t)-1) == wcstombs(mbstr, input, wcslen(input))) {
      free(mbstr);
      return (char *)(NULL);
    } else {
      return mbstr;
    }
  }
#else
  return input;
#endif /* !SALWIDECHARS */

}

int _Sal_RetStr(tstr_t out, const char *in) {

  int r = SAL_FALSE;

#ifdef SALWIDECHARS
  if (((size_t)-1) != mbstowcs(out, in, strlen(in))) {
    r = SAL_TRUE;
  }
#else
  strcpy(out, in);
  r = SAL_TRUE;
#endif /* !SALWIDECHARS */

  return r;
}

int _Sal_HasPendingConnect(const sal_t *s) {

  int r = SAL_FALSE;

  if (s->_f & SAL_F_PENDCONN) {
    r = SAL_TRUE;
  }

  return r;
}

int _Sal_IsClosedCircuit(const sal_t *s) {

  int r = SAL_FALSE;

  if (s) {

    unsigned char buf;

    int rcv = recv(s->sd, &buf, sizeof(unsigned char), MSG_PEEK);

    if (0 == rcv) { r = SAL_TRUE; }

    if (-1 == rcv) {

#if defined(_WIN32)

      int error = WSAGetLastError();

      if ((WSAENETDOWN      == error) ||
          (WSAENOTCONN      == error) ||
          (WSAEOPNOTSUPP    == error) ||
          (WSAESHUTDOWN     == error) ||
          (WSAECONNABORTED  == error) ||
          (WSAECONNRESET    == error))
      {
        r = SAL_TRUE;
      }

#else

      if ((EBADF    == errno) ||
          (ENOTCONN == errno) ||
          (ENOTSOCK == errno))
      {
        r = SAL_TRUE;
      }

#endif /* !_WIN32 */

    }

  }

  return r;
}

SALTHREADAPI _Sal_EventThread(void *p) {

  sal_eventthread_data *td = (sal_eventthread_data *)p;

  if (td) {

    sal_selectdata *t = NULL;
    fd_set r;
    fd_set w;
    fd_set e;

    while (0 == td->die) {

      if (SAL_TRUE == _Sal_Mutex_Lock(td->m)) {

        int size = _Sal_SDL_Size(td->l);
          
        if (0 < size) {
          
          struct timeval tv = {0, 0};
          int ret           = -1;
          sd_t highsd       = (sd_t)-1;

          FD_ZERO(&r);
          FD_ZERO(&w);
          FD_ZERO(&e);

          _Sal_SDL_Reset(td->l);

          while (SAL_TRUE == _Sal_SDL_Enum(td->l, &t)) {

            if (SAL_TRUE == _Sal_HasPendingConnect(t->s)) {

              t->mask |= SAL_S_CONNECT;
              t->s->_f &= ~SAL_F_PENDCONN;

            }

            if (t->s->sd > highsd) {
              highsd = t->s->sd;
            }

            FD_SET(t->s->sd, &r);
            FD_SET(t->s->sd, &w);
            FD_SET(t->s->sd, &e);

          }

          ret = select((int)(highsd + 1), &r, &w, &e, &tv);

          if ((-1 != ret)) {

            _Sal_DispatchEvents(&r, td, SAL_S_READ);
            _Sal_DispatchEvents(&w, td, SAL_S_WRITE);
            _Sal_DispatchEvents(&e, td, SAL_S_EXCEPT);

          }

        }

        _Sal_Mutex_Unlock(td->m);

      }

      FD_ZERO(&r);
      FD_ZERO(&w);
      FD_ZERO(&e);

#if defined(_WIN32)
      Sleep(1);
#else
      sched_yield();
#endif

    }

  } else {

#if defined(_WIN32)
    return 1;
#else
    return (void *)(1);
#endif

  }

#if defined(_WIN32)
  return 0;
#else
  return NULL;
#endif

}

int _Sal_InitAsyncSelect(sal_thread_t *t, sal_mutex_t *m, sal_eventthread_data *td) {

  int r = SAL_FALSE;

  if ((t) && (m) && (td)) {

    td->die = 0;

    if (SAL_TRUE == _Sal_Mutex_Init(m)) {

#if defined(_WIN32)

      DWORD id;

      if (NULL != ((*t) = CreateThread(NULL, 0, _Sal_EventThread, td, 0, &id))) {
        r = SAL_TRUE;
      }

#else

      if (0 == pthread_create(t, NULL, _Sal_EventThread, td)) {
        r = SAL_TRUE;
      }

#endif /* _WIN32 */

    }

  }

  return r;
}

void _Sal_DispatchEvents(fd_set *set, sal_eventthread_data *td, int type) {

  if ((set) && (td)) {

    sal_selectdata *t = NULL;

    _Sal_SDL_Reset(td->l);

    while (SAL_TRUE == _Sal_SDL_Enum(td->l, &t)) {

      int event = 0;
      int snd   = 0;

      if (0 != FD_ISSET(t->s->sd, set)) {

        switch (type) {

          case SAL_S_READ:

            if (t->mask & SAL_E_READ) {

              if (SAL_TRUE == _Sal_IsClosedCircuit(t->s)) {

                event = SAL_E_CLOSE;

              } else if (SAL_TRUE == Sal_IsListening(t->s)) {

                event = SAL_E_ACCEPT;

              } else {

                event = SAL_E_READ;

              }

              snd = 1;

            }

          break;
          case SAL_S_WRITE:

            if (t->mask & SAL_E_WRITE) {

              if (t->mask & SAL_S_CONNECT) {

                event    = SAL_E_CONNECT;
                t->mask &= ~SAL_S_CONNECT;

              } else {

                event = SAL_E_WRITE;

              }

              snd = 1;

            }

          break;
          case SAL_S_EXCEPT:

            if (t->mask & SAL_S_CONNECT) {

              event   = SAL_E_CONNFAIL;
              t->mask &= ~SAL_S_CONNECT;


            } else {

              event = SAL_E_EXCEPTION;

            }
            
            snd = 1;

          break;

        }

        if (1 == snd) {

          t->proc(t->s, event);

        }

        if (SAL_E_CLOSE == event) {

          _Sal_SDL_Rem(td->l, t->s->sd);

        }

      }

    }

  }

}

int _Sal_SDL_Add(sal_selectdata_list *l, const sal_selectdata *d) {

  int r = SAL_FALSE;

  if ((l) && (d)) {

    sal_selectdata **t  = NULL;
    sal_selectdata *n   = NULL;

    if (l->_h) {
      t = &l->_t->_n;
      n = l->_t;
    } else {
      t = &l->_h;
    }

    (*t) = (sal_selectdata *)malloc(sizeof(sal_selectdata));

    if ((*t)) {

      memcpy((*t), d, sizeof(sal_selectdata));

      (*t)->_n  = NULL;
      (*t)->_p  = n;
      l->_t     = (*t);
      r         = SAL_TRUE;

    }

  }

  return r;
}

int _Sal_SDL_Rem(sal_selectdata_list *l, sd_t sd) {

  int r = SAL_FALSE;

  if ((l) && (sd)) {

    sal_selectdata *t = _Sal_SDL_Find(l, sd);

    if (t) {

      if (t == l->_h) {

        l->_h = t->_n;

      } else {

        t->_p->_n = (t->_n ? t->_n : NULL);

        if (t == l->_t) {

          l->_t = t->_p;

        } else {

          t->_n->_p = (t->_p ? t->_p : NULL);

        }

      }

      free(t);
      r = SAL_TRUE;

    }

  }

  return r;
}

int _Sal_SDL_Clr(sal_selectdata_list *l) {

  int r = SAL_FALSE;

  if (l) {

    if (l->_h) {
      
      sal_selectdata *t   = l->_h;
      sal_selectdata *t2  = NULL;

      while (t) {

        t2 = t->_n;

        free(t);
       
        t = t2;
      }

      l->_h = l->_t = l->_c = NULL;
      r                     = SAL_TRUE;

    }

  }

  return r;
}

int _Sal_SDL_Size(sal_selectdata_list *l) {

  int r = 0;

  if (l) {

    sal_selectdata *d = l->_h;

    while (d) {

      d = d->_n;
      r++;

    }

  }

  return r;
}

int _Sal_SDL_Copy(sal_selectdata_list *dest, sal_selectdata_list *src) {

  int r = SAL_FALSE;

  if ((dest) && (src)) {

    sal_selectdata *d = src->_h;
    int copied        = 0;

    _Sal_SDL_Clr(dest);
    _Sal_SDL_Reset(src);

    while (d) {

      if (SAL_TRUE == _Sal_SDL_Add(dest, d)) {

        copied++;

      }

      d = d->_n;
    }

    if (0 < copied) {

      r = SAL_TRUE;

    }

  }

  return r;
}

int _Sal_SDL_Enum(sal_selectdata_list *l, sal_selectdata **d) {

  int r = SAL_FALSE;

  if ((l) && (d)) {

    if (l->_c) {

      *d    = l->_c;
      l->_c = l->_c->_n;
      r     = SAL_TRUE;

    } else {

      _Sal_SDL_Reset(l);

    }

  }

  return r;
}

void _Sal_SDL_Reset(sal_selectdata_list *l) {

  if (l) {
    l->_c = l->_h;
  }

}

sal_selectdata *_Sal_SDL_Find(const sal_selectdata_list *l, sd_t sd) {

  sal_selectdata *r = NULL;

  if ((l) && (sd)) {

    sal_selectdata *t = l->_h;

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

int _Sal_Mutex_Init(sal_mutex_t *m) {

  int r = SAL_FALSE;

  if (m) {

#if defined(_WIN32)

    *m = CreateMutex(NULL, FALSE, NULL);

    if (*m) {
      r = SAL_TRUE;
    }

#else

    if (0 == pthread_mutex_init(m, NULL)) {
      r = SAL_TRUE;
    }

#endif /* !_WIN32 */

  }

  return r;
}

int _Sal_Mutex_Lock(sal_mutex_t *m) {

  int r = SAL_FALSE;

  if (m) {

#if defined(_WIN32)

    if (WAIT_OBJECT_0 == WaitForSingleObject(*m, INFINITE)) {
      r = SAL_TRUE;
    }

#else

    if (0 == pthread_mutex_lock(m)) {
      r = SAL_TRUE;
    }

#endif /* !_WIN32 */

  }

  return r;
}

int _Sal_Mutex_Unlock(sal_mutex_t *m) {

  int r = SAL_FALSE;

  if (m) {

#if defined(_WIN32)

    if (ReleaseMutex(*m)) {
      r = SAL_TRUE;
    }

#else

    if (0 == pthread_mutex_unlock(m)) {
      r = SAL_TRUE;
    }

#endif /* !_WIN32 */

  }

  return r;
}

int _Sal_Mutex_Free(sal_mutex_t *m) {

  int r = SAL_FALSE;

  if (m) {

#if defined(_WIN32)

    if (CloseHandle(*m)) {
      r = SAL_TRUE;
    }

#else

    if (0 == pthread_mutex_destroy(m)) {
      r = SAL_TRUE;
    }

#endif /* !_WIN32 */

  }

  return r;

}
