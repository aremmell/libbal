/*
 * balerrors.c
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
#include "balplatform.h"
#include "balerrors.h"
#include "balinternal.h"

#if defined(BAL_SELFLOG)
void __bal_selflog(const char* func, const char* file, uint32_t line,
    const char* format, ...)
{
    va_list args;
    va_list args2;
    va_start(args, format);
    va_copy(args2, args);

    int prnt_len = vsnprintf(NULL, 0, format, args);

    va_end(args);
    BAL_ASSERT(prnt_len > 0);

    char* buf = calloc(prnt_len + 1, sizeof(char));
    BAL_ASSERT(NULL != buf);

    if (buf) {
        char prefix[256] = {0};
        int pfx_len = snprintf(prefix, 256, "%s (%s:%"PRIu32"): ", func, file, line);
        BAL_ASSERT(pfx_len > 0 && pfx_len < 256);

        va_start(args2, format);
        (void)vsnprintf(buf, prnt_len + 1, format, args2);
        va_end(args2);

        printf("%s%s\n", prefix, buf);

        bal_safefree(&buf);
    }
}
#endif
