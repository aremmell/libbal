/*
 * helpers.h
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

# include "types.h"
# include "errors.h"

/** Sets the specified pointer-to-pointer's value to NULL after free(). */
static inline
void __bal_safefree(void** pp)
{
    if (pp && *pp) {
        free(*pp);
        *pp = NULL;
    }
}

/** Coalesces types into void** for use by __bal_safefree. */
# define _bal_safefree(pp) __bal_safefree((void**)(pp))

/** Whether or not a particular bit or set of bits are set in a bitmask. */
# define bal_isbitset(bitmask, bit) (((bitmask) & (bit)) == (bit))

/** Sets the specified bits to one in the target bitmask. */
# define bal_setbitshigh(pbitmask, bits) ((*(pbitmask)) |= (bits))

/** Sets the specified bits to zero in the target bitmask. */
# define bal_setbitslow(pbitmask, bits) ((*(pbitmask)) &= ~(bits))

/** Returns the number of entries in an array. */
# define _bal_countof(arr) (sizeof((arr)) / sizeof((arr)[0]))

/** Use when truncation is expected/probable. Prevents truncation warnings
 * from GCC in particular. */
# define _bal_snprintf_trunc(dst, size, ...) \
    do { \
      volatile size_t n = size; \
      (void)snprintf(dst, n, __VA_ARGS__); \
    } while (false)

/** Allows a parameter to be unreferenced without compiler warnings. */
# define BAL_UNUSED(var) (void)(var)

/** getnameinfo flags: do not perform DNS queries. */
# define _BAL_NI_NODNS (NI_NUMERICHOST | NI_NUMERICSERV)

/** getnameinfo flags: perform DNS queries. */
# define _BAL_NI_DNS   (NI_NAMEREQD | NI_NUMERICSERV)

/** Returns the size of a sockaddr struct (IPv4/IPv6). */
# define _BAL_SASIZE(sa) \
    ((PF_INET6 == ((struct sockaddr* )&(sa))->sa_family) \
        ? sizeof(struct sockaddr_in6) : sizeof(struct sockaddr_in))

/** Locks the specified mutex and asserts that it was locked. */
# define _BAL_LOCK_MUTEX(m, name) \
    bool name##_locked = _bal_mutex_lock(m); \
    BAL_ASSERT_UNUSED(name##_locked, name##_locked)

/** Unlocks a mutex and asserts that it was unlocked. */
# define _BAL_UNLOCK_MUTEX(m, name) \
    bool name##_unlocked = _bal_mutex_unlock(m); \
    BAL_ASSERT_UNUSED(name##_unlocked, name##_unlocked)

#endif /* !_BAL_HELPERS_H_INCLUDED */
