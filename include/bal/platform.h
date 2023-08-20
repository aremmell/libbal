/*
 * platform.h
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
#   define __MACOS__
#   undef _DARWIN_C_SOURCE
#   define _DARWIN_C_SOURCE
#   define __HAVE_LIBC_STRLCPY__
#  elif defined(__linux__)
#   undef _GNU_SOURCE
#   define _GNU_SOURCE
#  elif defined (__FreeBSD__) || defined (__OpenBSD__) || defined (__NetBSD__) || \
        defined (__DragonFly__)
#   define __BSD__
#   undef _BSD_SOURCE
#   define _BSD_SOURCE
#   define __HAVE_LIBC_STRLCPY__
#  endif
#  if !defined(_POSIX_C_SOURCE)
#   define _POSIX_C_SOURCE 200809L
#  endif
#  if !defined(_DEFAULT_SOURCE)
#   define _DEFAULT_SOURCE
#  endif
#  if !defined(_XOPEN_SOURCE)
#   define _XOPEN_SOURCE 700
#  endif

# define __STDC_WANT_LIB_EXT1__ 1

#  if defined(__linux__)
#   include <sys/syscall.h>
#  elif defined(__sun)
#   include <sys/filio.h>
#   include <stropts.h>
#  endif

#  if defined(__linux__)
#   include <sys/syscall.h>
#  elif defined(__sun)
#   include <sys/filio.h>
#   include <stropts.h>
#  endif

#  include <sys/types.h>
#  include <sys/socket.h>
#  include <sys/select.h>
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

#  if !defined(__STDC_NO_ATOMICS__) && !defined(__cplusplus)
#   include <stdatomic.h>
#   undef __HAVE_STDATOMICS__
#   define __HAVE_STDATOMICS__
#  endif

#  define BAL_SOCKET_SPEC "%d"
#  define BAL_TID_SPEC "%x"

/** The socket descriptor type. */
typedef int bal_descriptor;

/** The type send/recv/sendto/recvfrom take for length. */
typedef size_t bal_iolen;

/** The type used in the linger struct. */
typedef int bal_linger;

/** The mutex type. */
typedef pthread_mutex_t bal_mutex;

/** The condition variable type. */
typedef pthread_cond_t bal_condition;

/** The thread handle type. */
typedef pthread_t bal_thread;

/** The mutex/condition variable wait time type. */
typedef struct timespec bal_wait;

/** The one-time type. */
typedef pthread_once_t bal_once;

/** The one-time execution function type. */
typedef void (*bal_once_fn)(void);

/** The thread callback return type. */
typedef void* bal_threadret;

/** The one-time initializer. */
#  define BAL_ONCE_INIT PTHREAD_ONCE_INIT

/** The thread initializer. */
#  define BAL_THREAD_INIT {0}

/** The mutex initializer. */
#  define BAL_MUTEX_INIT PTHREAD_MUTEX_INITIALIZER

#  define BAL_SHUT_RDWR SHUT_RDWR
#  define BAL_SHUT_RD   SHUT_RD
#  define BAL_SHUT_WR   SHUT_WR

# else /* _WIN32 */

#  define __WIN__
#  define _CRT_SECURE_NO_WARNINGS
#  define __WANT_STDC_SECURE_LIB__ 1
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  include <shlwapi.h>
#  include <process.h>
#  include <time.h>

#  undef __HAVE_STDATOMICS__

#  if defined(_MSC_VER) && _MSC_VER >= 1933 && !defined(__cplusplus)
#   include <stdatomic.h>
#   define __HAVE_STDATOMICS__
#  endif

#  define BAL_SOCKET_SPEC "%llu"
#  define BAL_TID_SPEC "%x"

/** The socket descriptor type. */
typedef SOCKET bal_descriptor;

/** The type send/recv/sendto/recvfrom take for length. */
typedef int bal_iolen;

/** The type used in the linger struct. */
typedef u_short bal_linger;

/** The mutex type. */
typedef CRITICAL_SECTION bal_mutex;

/** The condition variable type. */
typedef CONDITION_VARIABLE bal_condition;

/** The thread handle type. */
typedef uintptr_t bal_thread;

/** The mutex/condition variable wait time type. */
typedef DWORD bal_wait;

/** The one-time type. */
typedef INIT_ONCE bal_once;

/** The one-time execution function type. */
typedef BOOL(CALLBACK* bal_once_fn)(PINIT_ONCE, PVOID, PVOID*);

/** The process/thread identifier type. */
typedef int pid_t;

/** The thread callback return type. */
typedef unsigned bal_threadret;

/** The one-time initializer. */
#  define BAL_ONCE_INIT INIT_ONCE_STATIC_INIT

/** The thread initializer. */
#  define BAL_THREAD_INIT 0ULL

/** The mutex initializer. */
#  define BAL_MUTEX_INIT {0}

#  define BAL_SHUT_RDWR SD_BOTH
#  define BAL_SHUT_RD   SD_RECEIVE
#  define BAL_SHUT_WR   SD_SEND

#  define MSG_NOSIGNAL 0
#  define MSG_DONTWAIT 0

# endif /* !_WIN32 */

# include <stdio.h>
# include <stdarg.h>
# include <stdbool.h>
# include <stdint.h>
# include <inttypes.h>
# include <assert.h>

# define BAL_TRUE        0
# define BAL_FALSE      -1
# define BAL_BADSOCKET  -1
# define BAL_MAXERROR 1024

# define BAL_UNKNOWN "<unknown>"

# define BAL_AS_IPV6 "IPv6"
# define BAL_AS_IPV4 "IPv4"

# define BAL_EVT_READ     0x00000001U
# define BAL_EVT_WRITE    0x00000002U
# define BAL_EVT_CONNECT  0x00000004U
# define BAL_EVT_ACCEPT   0x00000008U
# define BAL_EVT_CLOSE    0x00000010U
# define BAL_EVT_CONNFAIL 0x00000020U
# define BAL_EVT_PRIORITY 0x00000040U
# define BAL_EVT_ERROR    0x00000080U
# define BAL_EVT_INVALID  0x00000100U
# define BAL_EVT_OOBREAD  0x00000200U
# define BAL_EVT_OOBWRITE 0x00000400U
# define BAL_EVT_ALL      0x000007ffU /**< Includes all available event types. */
# define BAL_EVT_NORMAL   0x00000bbdU /**< Excludes write, oob-write, priority. */

# define BAL_S_CONNECT    0x00000001U
# define BAL_S_LISTEN     0x00000002U
# define BAL_S_CLOSE      0x00000004U

# if defined(__MACOS__)
#  undef __HAVE_SO_ACCEPTCONN__
# else
#  define __HAVE_SO_ACCEPTCONN__
# endif

# if defined(__WIN__) && defined(__STDC_SECURE_LIB__)
#  define __HAVE_STDC_SECURE_OR_EXT1__
# elif defined(__STDC_LIB_EXT1__)
#  define __HAVE_STDC_SECURE_OR_EXT1__
# elif defined(__STDC_ALLOC_LIB__)
#  define __HAVE_STDC_EXT2__
# endif

# if defined(__MACOS__) || defined(__BSD__) || \
    (defined(_POSIX_C_SOURCE) && (_POSIX_C_SOURCE >= 200112L && !defined(_GNU_SOURCE)))
#  define __HAVE_XSI_STRERROR_R__
#  if defined(__GLIBC__)
#   if (__GLIBC__ >= 2 && __GLIBC_MINOR__ < 13)
#    define __HAVE_XSI_STRERROR_R_ERRNO__
#   endif
#  endif
# elif defined(_GNU_SOURCE) && defined(__GLIBC__)
#  define __HAVE_GNU_STRERROR_R__
# elif defined(__HAVE_STDC_SECURE_OR_EXT1__)
#  define __HAVE_STRERROR_S__
# endif

# if (__STDC_VERSION__ >= 201112 && !defined(__STDC_NO_THREADS__)) || \
     (defined(__SUNPRO_C) || defined(__SUNPRO_CC))
#  if defined(_AIX) && defined(__GNUC__)
#   define _bal_thread_local __thread
#  else
#   define _bal_thread_local _Thread_local
#  endif
# elif defined(__WIN__)
#  define _bal_thread_local __declspec(thread)
# elif defined(__GNUC__) || (defined(_AIX) && (defined(__xlC_ver__) || defined(__ibmxl__)))
#  define _bal_thread_local __thread
# else
#  error "unable to resolve thread local attribute; please contact the author."
# endif

# if (defined(__clang__) || defined(__GNUC__)) && defined(__FILE_NAME__)
#  define __file__ __FILE_NAME__
# elif defined(__BASE_FILE__)
#  define __file__ __BASE_FILE__
# else
#  define __file__ __FILE__
# endif

#endif /* !_BAL_PLATF0RM_H_INCLUDED */
