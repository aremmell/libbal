/*
 * balhelpers.h
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
#ifndef _BAL_HELPERS_H_INCLUDED
# define _BAL_HELPERS_H_INCLUDED

# include "balplatform.h"
# include "baltypes.h"

int _bal_aitoal(struct addrinfo* ai, bal_addrlist* out);
int _bal_retstr(char* out, const char* in, size_t destlen);

/** Prints a bal_socket to stdout. */
void _bal_socket_print(const bal_socket* s);

# define _BAL_SASIZE(sa) \
    ((PF_INET6 == ((struct sockaddr* )&(sa))->sa_family) \
        ? sizeof(struct sockaddr_in6) : sizeof(struct sockaddr_in))

static inline
void __bal_safefree(void** pp)
{
    if (pp && *pp) {
        free(*pp);
        *pp = NULL;
    }
}

# define _bal_safefree(pp) __bal_safefree((void**)(pp))

# define _bal_validptr(p) (NULL != (p))

# define _bal_validptrptr(pp) (NULL != (pp))

# define bal_countof(arr) (sizeof((arr)) / sizeof((arr)[0]))

# define _bal_validstr(str) ((str) && (*str))

# define _bal_validsock(s) (NULL != (s) && BAL_BADSOCKET != (s)->sd)

# define BAL_UNUSED(var) (void)(var)

# define _BAL_ENTER_MUTEX(m, name) \
    bool name##_locked = _bal_mutex_lock(m); \
    BAL_ASSERT(name##_locked); \
    if (name##_locked) {

# define _BAL_LEAVE_MUTEX(m, name) \
    bool name##_unlocked = _bal_mutex_unlock(m); \
    BAL_ASSERT_UNUSED(name##_unlocked, name##_unlocked); \
    }

#endif /* !_BAL_HELPERS_H_INCLUDED */
