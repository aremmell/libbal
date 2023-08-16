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

int _bal_getlasterror(bal_error* err);
bool __bal_setlasterror(int code, const char* func, const char* file,
    uint32_t line, bool gai);

# define _bal_handleerr(err)  \
    __bal_setlasterror(err, __func__, __file__, __LINE__, false)

# if defined(__WIN__)
#  define _bal_handlelasterr() \
    __bal_setlasterror(WSAGetLastError(), __func__, __file__, __LINE__, false)
# else
#  define _bal_handlelasterr() \
    __bal_setlasterror(errno, __func__, __file__, __LINE__, false)
# endif

# define _bal_handlegaierr(err) \
    __bal_setlasterror(err, __func__, __file__, __LINE__, true)

# if defined(BAL_DBGLOG)
void __bal_dbglog(const char* func, const char* file, uint32_t line,
    const char* format, ...);
#  define _bal_dbglog(...) \
    __bal_dbglog(__func__, __file__, __LINE__, __VA_ARGS__)
#  define BAL_ASSERT(...) \
    if (!(__VA_ARGS__)) { \
        __bal_dbglog(__func__, __file__, __LINE__, \
            "!!! assertion failed: %s", #__VA_ARGS__ ""); \
    }
# else
#  define _bal_dbglog(...)
#  define BAL_ASSERT(...)
# endif

# define BAL_ASSERT_UNUSED(var, expr) \
    if (!(expr)) { \
        BAL_ASSERT(expr); \
        BAL_UNUSED(var); \
    }

# if defined(__cplusplus)
}
# endif

#endif /* !_BAL_ERRORS_H_INCLUDED */
