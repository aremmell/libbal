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
#include "bal.h"
#include "balerrors.h"
#include "balinternal.h"

void __bal_dbglog(const char* func, const char* file, uint32_t line,
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
        int len = snprintf(prefix, 256, "["BAL_TID_SPEC"] %s (%s:%"PRIu32"): ",
            _bal_gettid(), func, file, line);
        BAL_ASSERT_UNUSED(len, len > 0 && len < 256);

        (void)vsnprintf(buf, prnt_len + 1, format, args2);
        va_end(args2);

        const char* color = "0";
#if defined(__WIN__)
        if (NULL != StrStrIA(buf, "error") || NULL != StrStrIA(buf, "assert"))
            color = "91";
        else if (NULL != StrStrIA(buf, "warn"))
            color = "33";
#else
        if (NULL != strcasestr(buf, "error") || NULL != strcasestr(buf, "assert"))
            color = "91";
        else if (NULL != strcasestr(buf, "warn"))
            color = "33";
#endif
        printf("\x1b[%sm%s%s\x1b[0m\n", color, prefix, buf);

        _bal_safefree(&buf);
    }
}

int _bal_getlasterror(const bal_socket* s, bal_error* err)
{
    int retval = 0;
    if (_bal_validptr(err)) {
        memset(err, 0, sizeof(bal_error));

        bool resolved = false;
        if (s) {
            err->code = bal_geterror(s);
            resolved  = -1 != err->code && 0 != err->code;
        }

#if defined(__WIN__)
        if (!resolved)
            err->code = WSAGetLastError();

        DWORD flags = FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
        if (0 != FormatMessageA(flags, NULL, err->code, 0U err->desc, BAL_MAXERROR, NULL))
            r = BAL_TRUE;
#else
        if (!resolved)
            err->code = errno;
# pragma message("TODO: these error codes are only appplicable when getaddrinfo or getnameinfo fail")
/*         switch (err->code) {
            case EAI_AGAIN:
            case EAI_FAIL:
            case EAI_MEMORY:
            case EAI_NODATA:
            case EAI_SOCKTYPE:
            case EAI_BADFLAGS:
            case EAI_BADHINTS:
            case EAI_FAMILY:
            case EAI_ADDRFAMILY:
            case EAI_NONAME:
            case EAI_SERVICE:
                if (0 == _bal_retstr(err->desc, gai_strerror(err->code), BAL_MAXERROR))
                    r = BAL_TRUE;
            break;
            default: */
                (void)_bal_retstr(err->desc, strerror(err->code), BAL_MAXERROR);

         /*    break;
        } */
#endif
        retval = err->code;
    }

    return retval;
}

bool _bal_setlasterror(int err)
{
#if defined(__WIN__)
    WSASetLastError(err);
#else
    errno = err;
#endif
    return false;
}
