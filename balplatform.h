/*
 * balplatform.h
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
#ifndef _BAL_PLATF0RM_H_INCLUDED
# define _BAL_PLATF0RM_H_INCLUDED

# if !defined(_WIN32)
#  if defined(__APPLE__) && defined(__MACH__)
#   undef _DARWIN_C_SOURCE
#   define _DARWIN_C_SOURCE
#  elif defined(__linux__)
#   undef _GNU_SOURCE
#   define _GNU_SOURCE
#  elif defined (__FreeBSD__)
#   undef _BSD_SOURCE
#   define _BSD_SOURCE
#  else
#  endif
#  undef _POSIX_C_SOURCE
#  define _POSIX_C_SOURCE 199309L
#  undef _DEFAULT_SOURCE
#  define _DEFAULT_SOURCE 1
#  undef _XOPEN_SOURCE
#  define _XOPEN_SOURCE 700

#  include <sys/types.h>
#  include <sys/socket.h>
#  include <sys/select.h> // TODO: get rid of select and exchange for poll
#  include <sys/time.h>
#  include <sys/ioctl.h>
#  include <netinet/in.h>
#  include <arpa/inet.h>
#  include <fcntl.h>
#  include <netdb.h>
#  include <unistd.h>
#  include <errno.h>
#  include <stdlib.h>
#  include <string.h>
#  include <pthread.h>
#  include <sched.h>
#  include <poll.h>

#  if defined(__sun)
#   include <sys/filio.h>
#   include <stropts.h>
#  endif

#  if !defined(__STDC_NO_ATOMICS__) && !defined(__cplusplus)
#   include <stdatomic.h>
#  endif

typedef int bal_descriptor;
typedef pthread_mutex_t bal_mutex;
typedef pthread_t bal_thread;

/** The one-time type. */
typedef pthread_once_t bal_once;

/** The one-time execution function type. */
typedef void (*bal_once_fn)(void);

/** The one-time initializer. */
#  define BAL_ONCE_INIT PTHREAD_ONCE_INIT

/** The thread initializer. */
#  define BAL_THREAD_INIT {0}

/** The mutex initializer. */
#  define BAL_MUTEX_INIT PTHREAD_MUTEX_INITIALIZER

# else /* _WIN32 */

#  define __WIN__
#  define _CRT_SECURE_NO_WARNINGS
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  include <process.h>
#  include <time.h>

#  undef __HAVE_ATOMIC_H__

# if !defined(__cplusplus) && defined(_MSC_VER) && _MSC_VER >= 1933
#  include <stdatomic.h>
#  define __HAVE_ATOMIC_H__
# endif

#  define WSOCK_MAJVER 2
#  define WSOCK_MINVER 2

/** The one-time type. */
typedef INIT_ONCE bal_once;

/** The one-time execution function type. */
typedef BOOL(CALLBACK* bal_once_fn)(PINIT_ONCE, PVOID, PVOID*);

/** The one-time initializer. */
#  define BAL_ONCE_INIT INIT_ONCE_STATIC_INIT

/** The thread initializer. */
#  define BAL_THREAD_INIT 0ull

/** The mutex initializer. */
#  define BAL_MUTEX_INIT INVALID_HANDLE_VALUE

typedef SOCKET bal_descriptor;
typedef HANDLE bal_mutex;
typedef uintptr_t bal_thread;
# endif

# include "version.h"

# include <stdio.h>
# include <stdarg.h>
# include <stdbool.h>
# include <stdint.h>
# include <assert.h>

typedef struct sockaddr_storage bal_sockaddr;

# define BAL_TRUE     0
# define BAL_FALSE    -1
# define BAL_MAXERROR 1024

# define BAL_AS_UNKNWN "<unknown>"
# define BAL_AS_IPV6   "IPv6"
# define BAL_AS_IPV4   "IPv4"

# define BAL_F_PENDCONN 0x00000001u
# define BAL_F_CLOSELCL 0x00000002u

# define BAL_BADSOCKET -1

# define _bal_validstr(str) (str && *str)

# define BAL_SASIZE(sa) \
    ((PF_INET == ((struct sockaddr* )&sa)->sa_family) ? sizeof(struct sockaddr_in) \
                                                      : sizeof(struct sockaddr_in6))

# define BAL_UNUSED(var) (void)var

# define BAL_NI_NODNS (NI_NUMERICHOST | NI_NUMERICSERV)
# define BAL_NI_DNS   (NI_NAMEREQD | NI_NUMERICSERV)

# define BAL_E_READ      0x00000001u
# define BAL_E_WRITE     0x00000002u
# define BAL_E_CONNECT   0x00000004u
# define BAL_E_ACCEPT    0x00000008u
# define BAL_E_CLOSE     0x00000010u
# define BAL_E_CONNFAIL  0x00000020u
# define BAL_E_EXCEPTION 0x00000040u
# define BAL_E_ALL       0x0000007Fu

# define BAL_S_DIE       0x0D1E0D1Eu
# define BAL_S_CONNECT   0x10000000u
# define BAL_S_READ      0x00000001u
# define BAL_S_WRITE     0x00000002u
# define BAL_S_EXCEPT    0x00000003u
# define BAL_S_TIME      0x0000C350u

#endif /* !_BAL_PLATF0RM_H_INCLUDED */