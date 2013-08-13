#ifndef _SAL_H_INCLUDED
#define _SAL_H_INCLUDED

#include <stdio.h>
#include <stdarg.h>

#if defined(_WIN32)

#include <Winsock2.h>
#include <Ws2tcpip.h>
#include <process.h>
#include <time.h>
#include <tchar.h>

#define WSOCK_MAJVER  2
#define WSOCK_MINVER  2

typedef SOCKET  sd_t;
typedef HANDLE  sal_mutex_t;
typedef HANDLE  sal_thread_t;

#else

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sched.h>

#if defined(__sun)
#include <sys/filio.h>
#include <stropts.h>
#endif

typedef int               sd_t;
typedef pthread_mutex_t   sal_mutex_t;
typedef pthread_t         sal_thread_t;

#endif /* !_WIN32 */

#ifdef SALWIDECHARS

#include <wchar.h>

#ifndef _t
#define _t(x) L ## x
#endif
typedef wchar_t tchar_t, *tstr_t;
typedef const wchar_t *ctstr_t;

#else

#ifndef _t
#define _t(x) x
#endif
typedef char tchar_t, *tstr_t;
typedef const char *ctstr_t;

#endif /* !SALWIDECHARS */

#define SAL_TRUE         0
#define SAL_FALSE       -1
#define SAL_ERROR       SAL_FALSE
#define SAL_MAXERRSTR   1024

#define SAL_AS_UNKNWN   _t("<unknown>")
#define SAL_AS_IPV6     _t("IPv6")
#define SAL_AS_IPV4     _t("IPv4")

#define SAL_F_PENDCONN  0x00000001UL
#define SAL_F_CLOSELCL  0x00000002UL

#define SAL_BADSOCKET   -1

typedef struct sockaddr_storage sal_sockaddr;

typedef struct {

  sd_t sd;
  int af;
  int st;
  int pf;
  unsigned long ud;
  unsigned long _f;

} sal_t;

typedef struct {

  struct addrinfo *_ai;
  struct addrinfo *_p;

} sal_addrinfo;

typedef struct sal_addr {

  sal_sockaddr     _sa;
  struct sal_addr *_n;

} sal_addr;

typedef struct {

  sal_addr *_a;
  sal_addr *_p;

} sal_addrlist;

typedef struct {

  tchar_t host[NI_MAXHOST];
  tchar_t ip[NI_MAXHOST];
  tstr_t  type;
  tchar_t port[NI_MAXSERV];

} sal_addrstrings;

typedef struct {

  int     code;
  tchar_t desc[SAL_MAXERRSTR];

} sal_error;

typedef void (*SALEVENTPROC)(const sal_t *, int);

typedef struct sal_selectdata {

  sal_t *s;
  unsigned long mask;
  SALEVENTPROC proc;
  struct sal_selectdata *_p;
  struct sal_selectdata *_n;

} sal_selectdata;

typedef struct {
  
  sal_selectdata *_h;
  sal_selectdata *_t;
  sal_selectdata *_c;

} sal_selectdata_list;

typedef struct {

  sal_selectdata_list *l;
  sal_mutex_t         *m;
  int                  die;

} sal_eventthread_data;

#ifdef __cplusplus
extern "C" {
#endif

int Sal_Initialize(void);
int Sal_Finalize(void);

int Sal_AsyncSelect(const sal_t *s, SALEVENTPROC proc, unsigned long mask);

int Sal_AutoSocket(sal_t *s, int af, int pt, ctstr_t host, ctstr_t port);
int Sal_Socket(sal_t *s, int af, int pt, int st);
int Sal_Reset(sal_t *s);
int Sal_Close(sal_t *s);
int Sal_ShutDown(sal_t *s, int how);

int Sal_Connect(const sal_t *s, ctstr_t host, ctstr_t port);
int Sal_ConnectAddrList(sal_t *s, sal_addrlist *al);

int Sal_Send(const sal_t *s, const void *data, size_t len, int flags);
int Sal_Recv(const sal_t *s, void *data, size_t len, int flags);

int Sal_SendTo(const sal_t *s, ctstr_t host, ctstr_t port, const void *data, size_t len, int flags);
int Sal_SendToAddr(const sal_t *s, const sal_sockaddr *sa, const void *data, size_t len, int flags);

int Sal_RecvFrom(const sal_t *s, void *data, size_t len, int flags, sal_sockaddr *res);

int Sal_Bind(const sal_t *s, ctstr_t addr, ctstr_t port);
int Sal_BindAddrAny(const sal_t *s, unsigned short port);
int Sal_BindAny(const sal_t *s);

int Sal_Listen(const sal_t *s, int backlog);
int Sal_Accept(const sal_t *s, sal_t *res, sal_sockaddr *resaddr);

int Sal_GetOption(const sal_t *s, int level, int name, void *optval, socklen_t len);
int Sal_SetOption(const sal_t *s, int level, int name, const void *optval, socklen_t len);

int Sal_SetBroadcast(const sal_t *s, int flag);
int Sal_GetBroadcast(const sal_t *s);

int Sal_SetDebug(const sal_t *s, int flag);
int Sal_GetDebug(const sal_t *s);

int Sal_SetLinger(const sal_t *s, int sec);
int Sal_GetLinger(const sal_t *s, int *sec);

int Sal_SetKeepAlive(const sal_t *s, int flag);
int Sal_GetKeepAlive(const sal_t *s);

int Sal_SetOOBInline(const sal_t *s, int flag);
int Sal_GetOOBInline(const sal_t *s);

int Sal_SetReuseAddr(const sal_t *s, int flag);
int Sal_GetReuseAddr(const sal_t *s);

int Sal_SetSendBufSize(const sal_t *s, int size);
int Sal_GetSendBufSize(const sal_t *s);

int Sal_SetRecvBufSize(const sal_t *s, int size);
int Sal_GetRecvBufSize(const sal_t *s);

int Sal_SetSendTimeout(const sal_t *s, long sec, long msec);
int Sal_GetSendTimeout(const sal_t *s, long *sec, long *msec);

int Sal_SetRecvTimeout(const sal_t *s, long sec, long msec);
int Sal_GetRecvTimeout(const sal_t *s, long *sec, long *msec);

int Sal_GetError(const sal_t *s);

int Sal_IsListening(const sal_t *s);
int Sal_IsReadable(const sal_t *s);
int Sal_IsWritable(const sal_t *s);

int Sal_SetIOMode(const sal_t *s, unsigned long flag);
size_t Sal_RecvQueueSize(const sal_t *s);

int Sal_LastLibError(sal_error *err);
int Sal_LastSockError(const sal_t *s, sal_error *err);

int Sal_ResolveHost(ctstr_t host, sal_addrlist *out);
int Sal_GetRemoteHostAddr(const sal_t *s, sal_sockaddr *out);
int Sal_GetRemoteHostStrings(const sal_t *s, int dns, sal_addrstrings *out);
int Sal_GetLocalHostAddr(const sal_t *s, sal_sockaddr *out);
int Sal_GetLocalHostStrings(const sal_t *s, int dns, sal_addrstrings *out);

int Sal_ResetAddrList(sal_addrlist *al);
const sal_sockaddr *Sal_EnumAddrList(sal_addrlist *al);
int Sal_FreeAddrList(sal_addrlist *al);
int Sal_GetAddrStrings(const sal_sockaddr *in, int dns, sal_addrstrings *out);


/*
 * Internally used definitions
 */


#define _SETSTR(str) \
  ((NULL != str) && (0 != (*str)))

#define SAL_SASIZE(sa) \
  ((PF_INET == ((struct sockaddr *)&sa)->sa_family) \
   ? sizeof(struct sockaddr_in) : \
     sizeof(struct sockaddr_in6))

#define SAL_NI_NODNS    (NI_NUMERICHOST | NI_NUMERICSERV)
#define SAL_NI_DNS      (NI_NAMEREQD    | NI_NUMERICSERV)

#define SAL_E_READ      0x00000001
#define SAL_E_WRITE     0x00000002
#define SAL_E_CONNECT   0x00000004
#define SAL_E_ACCEPT    0x00000008
#define SAL_E_CLOSE     0x00000010
#define SAL_E_CONNFAIL  0x00000020
#define SAL_E_EXCEPTION 0x00000040
#define SAL_E_ALL       0x0000007F

#define SAL_S_DIE       0x0D1E0D1E
#define SAL_S_CONNECT   0x10000000
#define SAL_S_READ      0x00000001
#define SAL_S_WRITE     0x00000002
#define SAL_S_EXCEPT    0x00000003
#define SAL_S_TIME      0x0000C350

int _Sal_BindAny(const sal_t *s, unsigned short port);
int _Sal_GetAddrInfo(int f, int af, int st, ctstr_t host, ctstr_t port, sal_addrinfo *res);
int _Sal_GetNameInfo(int f, const sal_sockaddr *in, tstr_t host, tstr_t port);

const struct addrinfo *_Sal_EnumAddrInfo(sal_addrinfo *ai);
int _Sal_AIToAL(sal_addrinfo *in, sal_addrlist *out);

int _Sal_GetLastError(const sal_t *s, sal_error *err);
void _Sal_SetLastError(int err);

const char *_Sal_GetMBStr(ctstr_t input);
int _Sal_RetStr(tstr_t out, const char *in);

int _Sal_HasPendingConnect(const sal_t *s);
int _Sal_IsClosedCircuit(const sal_t *s);

#if defined(_WIN32)
#define SALTHREADAPI  DWORD WINAPI
#else
#define SALTHREADAPI  void *
#endif

SALTHREADAPI _Sal_EventThread(void *p);
int _Sal_InitAsyncSelect(sal_thread_t *t, sal_mutex_t *m, sal_eventthread_data *td);
void _Sal_DispatchEvents(fd_set *set, sal_eventthread_data *td, int type);

int _Sal_SDL_Add(sal_selectdata_list *l, const sal_selectdata *d);
int _Sal_SDL_Rem(sal_selectdata_list *l, sd_t sd);
int _Sal_SDL_Clr(sal_selectdata_list *l);
int _Sal_SDL_Size(sal_selectdata_list *l);
int _Sal_SDL_Enum(sal_selectdata_list *l, sal_selectdata **d);
void _Sal_SDL_Reset(sal_selectdata_list *l);
sal_selectdata *_Sal_SDL_Find(const sal_selectdata_list *l, sd_t sd);

int _Sal_Mutex_Init(sal_mutex_t *m);
int _Sal_Mutex_Lock(sal_mutex_t *m);
int _Sal_Mutex_Unlock(sal_mutex_t *m);
int _Sal_Mutex_Free(sal_mutex_t *m);

#ifdef __cplusplus
}
#endif

/*
 * C++
 */
#ifdef __cplusplus

#include <string>
#include <list>

#define SAL_RV(r) ((SAL_TRUE == r) ? true : false)
#define SAL_BI(b) ((b) ? 1 : 0)

typedef std::basic_string<tchar_t> t_string;
typedef const std::basic_string<tchar_t> t_cstring;

namespace Sal {

class AddrInfo {

public:
  AddrInfo() {}
  AddrInfo(const sal_sockaddr *sa, bool dns = false) {
    Set(sa, dns);
  }
  ~AddrInfo() {}

  bool Set(const sal_sockaddr *sa, bool dns = false) {

    bool r = false;

    if (sa) {
      
      sal_addrstrings as;

      memcpy(&m_sa, sa, sizeof(sal_sockaddr));

      if (SAL_TRUE == Sal_GetAddrStrings(&m_sa, SAL_BI(dns), &as)) {

        m_host  = as.host;
        m_ip    = as.ip;
        m_port  = as.port;
        m_type  = as.type;
        r       = true;

      } else {
        Reset();
      }

    }

    return r;
  }

  void Reset(void) {

    memset(&m_sa, 0, sizeof(sal_sockaddr));

    m_host  = _t("");
    m_ip    = _t("");
    m_port  = _t("");
    m_type  = _t("");

  }

  t_cstring &HostName(void) {
    return const_cast<t_cstring &>(m_host);
  }

  t_cstring &IPAddress(void) {
    return const_cast<t_cstring &>(m_ip);
  }

  t_cstring &Port(void) {
    return const_cast<t_cstring &>(m_port);
  }

  t_cstring &Type(void) {
    return const_cast<t_cstring &>(m_type);
  }

  const sal_sockaddr *Binary(void) {
    return const_cast<const sal_sockaddr *>(&m_sa);
  }

private:
  sal_sockaddr m_sa;
  t_string     m_host;
  t_string     m_ip;
  t_string     m_port;
  t_string     m_type;
  
};

class AddrList : public std::list<AddrInfo> {

public:
  AddrList() {}
  virtual ~AddrList() {}

  void Reset(void) {
    m_it = begin();
  }

  bool Enumerate(AddrInfo **sai) {

    bool r = false;

    if (sai) {
      if (m_it != end()) {
        *sai = &(*m_it);
        m_it++;
        r = true;
      } else {
        Reset();
      }
    }

    return r;
  }

protected:
  std::list<AddrInfo>::iterator m_it;
};

class Error {

public:
  Error() {}
  ~Error() {}

  bool Set(const sal_error *err) {

    bool r = false;

    if (err) {

      memcpy(&m_e, err, sizeof(sal_error));

      m_desc  = m_e.desc;
      r       = true;

    } else {

      memset(&m_e, 0, sizeof(sal_error));

      m_desc  = _t("");
      r       = true;

    }

    return r;
  }

  t_cstring &Description(void) {
    return const_cast<t_cstring &>(m_desc);
  }

  int Code(void) {
    return m_e.code;
  }

private:
  sal_error m_e;
  t_string  m_desc;

};

class Socket {

public:
  Socket() {
    Sal_Initialize();
  }
  virtual ~Socket() {
    Sal_Finalize();
  }

  bool AutoCreate(t_cstring &host, t_cstring &port, int af = 0, int pt = 0);
  bool Create(int af, int pt, int st);
  bool Reset(void);
  bool Close(void);
  bool ShutDown(int how);

  bool Connect(t_cstring &host, t_cstring &port);
  bool Connect(AddrList &al);

  int Send(const void *data, size_t len, int flags = 0);
  int Recv(void *data, size_t len, int flags = 0);

  int SendTo(t_cstring &host, t_cstring &port, const void *data, size_t len, int flags = 0);
  int SendTo(AddrInfo &sa, const void *data, size_t len, int flags = 0);

  int RecvFrom(void *data, size_t len, int flags = 0, AddrInfo *res = NULL);

  bool Bind(t_cstring &addr, t_cstring &port);
  bool Listen(int backlog);
  bool Accept(class Socket &res, AddrInfo &resaddr);

  bool GetOption(int level, int name, void *optval, socklen_t len);
  bool SetOption(int level, int name, const void *optval, socklen_t len);

  bool SetBroadcast(bool flag);
  bool UsesBroadcast(void);

  bool SetDebug(bool flag);
  bool IsDebuggingEnabled(void);

  bool SetLinger(int sec);
  bool GetLinger(int *sec);

  bool SetKeepAlive(bool flag);
  bool UsesKeepAlive(void);

  bool SetOOBInline(bool flag);
  bool UsesOOBInline(void);

  bool SetReuseAddr(bool flag);
  bool UsesReuseAddr(void);

  bool SetSendBufSize(int size);
  int  GetSendBufSize(void);

  bool SetRecvBufSize(int size);
  int  GetRecvBufSize(void);

  bool SetSendTimeout(long sec, long msec);
  bool GetSendTimeout(long *sec, long *msec);

  bool SetRecvTimeout(long sec, long msec);
  bool GetRecvTimeout(long *sec, long *msec);

  int  GetError(void);

  bool IsListening(void);
  bool IsReadable(void);
  bool IsWritable(void);

  bool SetIOMode(bool flag);
  size_t RecvQueueSize(void);

  bool LastLibError(Error &err);
  bool LastSockError(Error &err);

  bool ResolveHost(t_cstring &host, AddrList &out, bool names = false);

  bool GetRemoteHostInfo(AddrInfo &res, bool dns = true);
  bool GetLocalHostInfo(AddrInfo &res, bool dns = true);

  sd_t Descriptor(void) {
    return m_s.sd;
  }

protected:
  sal_t m_s;

  const sal_t *_cs(void) {
    return const_cast<const sal_t *>(&m_s);
  }
  
  void _Set(const sal_t *s) {
    Reset();
    memcpy(&m_s, s, sizeof(sal_t));
  }

};

}; /* !Sal namespace */

#endif /* !__cplusplus */

#endif /* !_SAL_H_INCLUDED */
