/*
 * balerrors.h
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

# if defined(__cplusplus)
extern "C" {
# endif

# define BAL_UNUSED(var) (void)(var)

# if defined(BAL_SELFLOG)
void __bal_selflog(const char* func, const char* file, uint32_t line,
    const char* format, ...);
#  define _bal_selflog(...) \
    __bal_selflog(__func__, __file__, __LINE__, __VA_ARGS__);
# else
static inline
void __dummy_func(const char* dummy, ...) { BAL_UNUSED(dummy); }
#  define _bal_selflog(...) __dummy_func(__VA_ARGS__)
# endif

# if defined(BAL_SELFLOG)
#  define BAL_ASSERT(...) \
    if (!(__VA_ARGS__)) { \
        _bal_selflog("!!! assertion failed: %s", #__VA_ARGS__); \
    }
# else
#  define BAL_ASSERT(...) BAL_UNUSED(__VA_ARGS__)
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
