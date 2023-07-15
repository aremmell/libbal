/*
 * bal.hh
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
#ifndef _BAL_HH_INCLUDED
# define _BAL_HH_INCLUDED

# include "bal.h"

# include <string>
# include <list>

# define BAL_RV(r) ((BAL_TRUE == r) ? true : false)
# define BAL_BI(b) ((b) ? 1 : 0)

typedef std::basic_string<bchar> t_string;
typedef const std::basic_string<bchar> t_cstring;

namespace bal
{
    class AddrInfo
    {
      public:
        AddrInfo() {}
        AddrInfo(const bal_sockaddr *sa, bool dns = false) {
            Set(sa, dns);
        }
        ~AddrInfo() {}

        bool Set(const bal_sockaddr *sa, bool dns = false) {
            bool r = false;

            if (sa) {
                bal_addrstrings as;

                memcpy(&m_sa, sa, sizeof(bal_sockaddr));

                if (BAL_TRUE == Bal_GetAddrStrings(&m_sa, BAL_BI(dns), &as)) {
                    m_host = as.host;
                    m_ip   = as.ip;
                    m_port = as.port;
                    m_type = as.type;
                    r      = true;

                } else {
                    Reset();
                }
            }

            return r;
        }

        void Reset(void) {
            memset(&m_sa, 0, sizeof(bal_sockaddr));

            m_host = _t("");
            m_ip   = _t("");
            m_port = _t("");
            m_type = _t("");
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

        const bal_sockaddr *Binary(void) {
            return const_cast<const bal_sockaddr *>(&m_sa);
        }

      private:
        bal_sockaddr m_sa;
        t_string m_host;
        t_string m_ip;
        t_string m_port;
        t_string m_type;
    };

    class AddrList : public std::list<AddrInfo>
    {
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

    class Error
    {
      public:
        Error() {}
        ~Error() {}

        bool Set(const bal_error *err) {
            bool r = false;

            if (err) {
                memcpy(&m_e, err, sizeof(bal_error));

                m_desc = m_e.desc;
                r      = true;

            } else {
                memset(&m_e, 0, sizeof(bal_error));

                m_desc = _t("");
                r      = true;
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
        bal_error m_e;
        t_string m_desc;
    };

    class Socket
    {
      public:
        Socket() {
            Bal_Initialize();
        }
        virtual ~Socket() {
            Bal_Finalize();
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
        int GetSendBufSize(void);

        bool SetRecvBufSize(int size);
        int GetRecvBufSize(void);

        bool SetSendTimeout(long sec, long msec);
        bool GetSendTimeout(long *sec, long *msec);

        bool SetRecvTimeout(long sec, long msec);
        bool GetRecvTimeout(long *sec, long *msec);

        int GetError(void);

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

        bal_socket Descriptor(void) {
            return m_s.sd;
        }

      protected:
        bal_t m_s;

        const bal_t *_cs(void) {
            return const_cast<const bal_t *>(&m_s);
        }

        void _Set(const bal_t *s) {
            Reset();
            memcpy(&m_s, s, sizeof(bal_t));
        }
    };
}; // namespace bal


#endif /* !_BAL_HH_INCLUDED */
