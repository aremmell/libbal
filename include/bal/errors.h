/*
 * errors.h
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
#ifndef _BAL_ERRORS_H_INCLUDED
# define _BAL_ERRORS_H_INCLUDED

# include "types.h"

# if defined(__cplusplus)
extern "C" {
# endif

int _bal_get_error(bal_error* err, bool extended);
bool __bal_set_error(int code, const char* func, const char* file, uint32_t line);
void __bal_set_os_error(int code, const char* message, const char* func,
    const char* file, uint32_t line);
bool __bal_handle_error(int code, const char* func, const char* file,
    uint32_t line, bool gai);

/** Creates a libbal-specific error code from a positive integer that would
 * otherwise likely collide with OS-level error codes. Supports values
 * 1..255 inclusive. */
# define _bal_mk_error(err) ((((err) & 0xff) << 16) | 0x78000000)

/** libbal-specific error codes. */
# define BAL_E_NOERROR      1 /**< Operation completed successfully */
# define BAL_E_NULLPTR      2 /**< NULL pointer argument */
# define BAL_E_BADSTRING    3 /**< Invalid string argument */
# define BAL_E_BADSOCKET    4 /**< Invalid bal_socket argument */
# define BAL_E_BADBUFLEN    5 /**< Invalid buffer length argument */
# define BAL_E_INVALIDARG   6 /**< Invalid argument */
# define BAL_E_NOTINIT      7 /**< libbal is not initialized */
# define BAL_E_DUPEINIT     8 /**< libbal is already initialized */
# define BAL_E_ASNOTINIT    9 /**< Asynchronous I/O is not initialized */
# define BAL_E_ASDUPEINIT  10 /**< Asynchronous I/O is already initialized */
# define BAL_E_ASNOSOCKET  11 /**< Socket is not registered for asynchronous I/O events */
# define BAL_E_BADEVTMASK  12 /**< Invalid asynchronous I/O event bitmask */
# define BAL_E_INTERNAL    13 /**< An internal error has occurred */
# define BAL_E_UNAVAIL     14 /**< Feature is disabled or unavailable */
# define BAL_E_PLATFORM    15 /**< Platform error code %d (%s) */
# define BAL_E_UNKNOWN    255 /**< An unknown error has occurred */

/** libbal-specific packed error code values. */
# define _BAL_E_NOERROR    _bal_mk_error(BAL_E_NOERROR)
# define _BAL_E_NULLPTR    _bal_mk_error(BAL_E_NULLPTR)
# define _BAL_E_BADSTRING  _bal_mk_error(BAL_E_BADSTRING)
# define _BAL_E_BADSOCKET  _bal_mk_error(BAL_E_BADSOCKET)
# define _BAL_E_BADBUFLEN  _bal_mk_error(BAL_E_BADBUFLEN)
# define _BAL_E_INVALIDARG _bal_mk_error(BAL_E_INVALIDARG)
# define _BAL_E_NOTINIT    _bal_mk_error(BAL_E_NOTINIT)
# define _BAL_E_DUPEINIT   _bal_mk_error(BAL_E_DUPEINIT)
# define _BAL_E_ASNOTINIT  _bal_mk_error(BAL_E_ASNOTINIT)
# define _BAL_E_ASDUPEINIT _bal_mk_error(BAL_E_ASDUPEINIT)
# define _BAL_E_ASNOSOCKET _bal_mk_error(BAL_E_ASNOSOCKET)
# define _BAL_E_BADEVTMASK _bal_mk_error(BAL_E_BADEVTMASK)
# define _BAL_E_INTERNAL   _bal_mk_error(BAL_E_INTERNAL)
# define _BAL_E_UNAVAIL    _bal_mk_error(BAL_E_UNAVAIL)
# define _BAL_E_PLATFORM   _bal_mk_error(BAL_E_PLATFORM)
# define _BAL_E_UNKNOWN    _bal_mk_error(BAL_E_UNKNOWN)

/** Determines if the input is a packed error created by _bal_mk_error. */
static inline
bool _bal_is_error(int err)
{
    int masked = (err & 0x78ff0000);
    return masked >= 0x78010000 && masked <= 0x78ff0000;
}

/** Extracts the libbal-specific error code from a packed error created by
 * _bal_mk_error. */
static inline
int _bal_err_code(int err)
{
    return ((err >> 16) & 0x000000ff);
}

static inline
bool __bal_validate(bool expr, int err, const char* func, const char* file,
    uint32_t line)
{
    if (!expr && 0 != err)
        __bal_set_error(err, func, file, line);
    return expr;
}

# define _bal_okptrnf(p) \
    __bal_validate(NULL != (p), 0, __func__, __file__, __LINE__)

# define _bal_okptr(p) \
    __bal_validate(NULL != (p), _BAL_E_NULLPTR, __func__, __file__, __LINE__)

# define _bal_okptrptrnf(pp) \
    __bal_validate(NULL != (pp), 0, __func__, __file__, __LINE__)

# define _bal_okptrptr(pp) \
    __bal_validate(NULL != (pp), _BAL_E_NULLPTR, __func__, __file__, __LINE__)

# define _bal_okstrnf(str) \
    __bal_validate(NULL != (str) && '\0' != *(str), 0, __func__, __file__, __LINE__)

# define _bal_okstr(str) \
    __bal_validate(NULL != (str) && '\0' != *(str), _BAL_E_BADSTRING, __func__, \
        __file__, __LINE__)

# define _bal_oksocknf(s) \
    __bal_validate(NULL != (s) && -1 != (s)->sd, 0, __func__, __file__, __LINE__)

# define _bal_oksock(s) \
    __bal_validate(NULL != (s) && -1 != (s)->sd, _BAL_E_BADSOCKET, __func__, \
        __file__, __LINE__)

# define _bal_oklennf(len) \
    __bal_validate((len) > 0, 0, __func__, __file__, __LINE__)

# define _bal_oklen(len) \
    __bal_validate((len) > 0, _BAL_E_BADBUFLEN, __func__, __file__, __LINE__)

# define _bal_seterror(err) \
    __bal_set_error(err, __func__, __file__, __LINE__)

# define _bal_handleerr(err) \
    __bal_handle_error(err, __func__, __file__, __LINE__, false)

# define _bal_handlesockerr(s) _bal_handleerr(bal_sock_get_error(s))

# if defined(__WIN__)
#  define _bal_handlelasterr() \
    __bal_handle_error(WSAGetLastError(), __func__, __file__, __LINE__, false)
# else
#  define _bal_handlelasterr() \
    __bal_handle_error(errno, __func__, __file__, __LINE__, false)
# endif

# define _bal_handlegaierr(err) \
    __bal_handle_error(err, __func__, __file__, __LINE__, true)

# if defined(BAL_DBGLOG)
void __bal_dbglog(const char* func, const char* file, uint32_t line,
    const char* format, ...);
#  define _bal_dbglog(...) \
    __bal_dbglog(__func__, __file__, __LINE__, __VA_ARGS__)
# else
#  define _bal_dbglog(...)
# endif

# if defined(BAL_ASSERT_ENABLED)
#  define BAL_ASSERT(...) assert(__VA_ARGS__)
# elif defined(BAL_DBGLOG)
#  define BAL_ASSERT(...) \
    do { \
        if (!(__VA_ARGS__)) { \
            __bal_dbglog(__func__, __file__, __LINE__, \
                "!!! assertion failed: %s", #__VA_ARGS__ ""); \
        } \
    } while (false)
# else
#  define BAL_ASSERT(...)
# endif

# define BAL_ASSERT_UNUSED(var, expr) \
    BAL_ASSERT(expr); \
    BAL_UNUSED(var)

# if defined(__cplusplus)
}
# endif

#endif /* !_BAL_ERRORS_H_INCLUDED */
