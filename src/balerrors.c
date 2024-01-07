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
#include "bal/errors.h"
#include "bal/internal.h"

#if defined(__WIN__)
# pragma comment(lib, "shlwapi.lib")
#endif

/** Container for information about the last error that occurred on this thread. */
static _bal_thread_local bal_thread_error_info _bal_tei = {
    _BAL_E_NOERROR, {BAL_UNKNOWN, BAL_UNKNOWN, 0U}, {0, BAL_UNKNOWN}
};

/** The string used to format error messages generated by libbal when
 * extended information (function, file, line) is requested. */
#define BAL_ERRFMTEXT "Error in %s (%s:%u): '%s'"

/** The string used to format error messages generated by libbal when
 * extended information is not requested. */
#define BAL_ERRFMT "%s"

/** The string used to format error messages originating from the OS. */
#define BAL_ERRFMTPFORM "Platform error code %d: %s"

/** Map of defined libbal-specific packed error code values <-> descriptions. */
static const struct {
    const int code;
    const char* const msg;
} bal_errors[] = {
    {_BAL_E_NOERROR,    "Operation completed successfully"},
    {_BAL_E_NULLPTR,    "NULL pointer argument"},
    {_BAL_E_BADSTRING,  "Invalid string argument"},
    {_BAL_E_BADSOCKET,  "Invalid bal_socket argument"},
    {_BAL_E_BADBUFLEN,  "Invalid buffer length argument"},
    {_BAL_E_INVALIDARG, "Invalid argument"},
    {_BAL_E_NOTINIT,    "libbal is not initialized"},
    {_BAL_E_DUPEINIT,   "libbal is already initialized"},
    {_BAL_E_ASNOTINIT,  "Asynchronous I/O is not initialized"},
    {_BAL_E_ASDUPEINIT, "Asynchronous I/O is already initialized"},
    {_BAL_E_ASNOSOCKET, "Socket is not registered for asynchronous I/O events"},
    {_BAL_E_BADEVTMASK, "Invalid asynchronous I/O event bitmask"},
    {_BAL_E_INTERNAL,   "An internal error has occurred"},
    {_BAL_E_UNAVAIL,    "Feature is disabled or unavailable"},
    {_BAL_E_PLATFORM,   BAL_ERRFMTPFORM},
    {_BAL_E_UNKNOWN,    "An unknown error has occurred"}
};

int _bal_get_error(bal_error* err, bool extended)
{
    int retval = -1;

    if (_bal_okptrnf(err)) {
        memset(err, 0, sizeof(bal_error));
        err->code = _bal_err_code(_BAL_E_UNKNOWN);
        for (size_t n = 0; n < _bal_countof(bal_errors); n++) {
            if (bal_errors[n].code == _bal_tei.code) {
                char* heap_msg = NULL;
                if (_BAL_E_PLATFORM == bal_errors[n].code) {
                    heap_msg = calloc(BAL_MAXERROR + 33, sizeof(char));
                    if (NULL != heap_msg) {
                        _bal_snprintf_trunc(heap_msg, BAL_MAXERROR + 33, bal_errors[n].msg,
                            _bal_tei.os.code, _bal_okstrnf(_bal_tei.os.msg)
                                ? _bal_tei.os.msg : BAL_UNKNOWN);
                    }
                }

                if (extended) {
                    _bal_snprintf_trunc(err->message, BAL_MAXERRORFMT, BAL_ERRFMTEXT,
                        _bal_tei.loc.func, _bal_tei.loc.file, _bal_tei.loc.line,
                        _bal_okptrnf(heap_msg) ? heap_msg : bal_errors[n].msg);
                } else {
                    _bal_snprintf_trunc(err->message, BAL_MAXERRORFMT, BAL_ERRFMT,
                        _bal_okptrnf(heap_msg) ? heap_msg : bal_errors[n].msg);
                }

                _bal_safefree(&heap_msg);
                retval = err->code = _bal_err_code(bal_errors[n].code);
                break;
            }
        }
    }

    return retval;
}

bool __bal_set_error(int code, const char* func, const char* file, uint32_t line)
{
    if (_bal_is_error(code)) {
        _bal_tei.code = code;
        _bal_tei.loc.func = func;
#if defined(__WIN__)
        const char* last_slash = StrRChrA(file, NULL, '\\');
        if (NULL == last_slash)
              last_slash = StrRChrA(file, NULL, '/');
#else
        const char* last_slash = strrchr(file, '/');
#endif
        if (NULL != last_slash)
            file = last_slash + 1;

        _bal_tei.loc.file = file;
        _bal_tei.loc.line = line;
    }

#if defined(BAL_DBGLOG) && defined(BAL_DBGLOG_SETERROR)
    if (0 != code) {
        bal_error err = {0};
        __bal_dbglog(func, file, line, "%d (%s)",
            _bal_get_error(&err, false), err.message);
    }
#endif
    return false;
}

void __bal_set_os_error(int code, const char* message, const char* func,
    const char* file, uint32_t line)
{
    _bal_tei.os.code = code;
    _bal_tei.os.msg[0] = '\0';

    if (_bal_okstrnf(message))
        _bal_strcpy(_bal_tei.os.msg, BAL_MAXERROR, message,
            strnlen(message, BAL_MAXERROR));

    (void)__bal_set_error(_BAL_E_PLATFORM, func, file, line);
}

bool __bal_handle_error(int code, const char* func, const char* file,
    uint32_t line, bool gai)
{
    char msg[BAL_MAXERROR] = {0};
#if defined(__WIN__)
    BAL_UNUSED(gai);

    DWORD flags = FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS |
                    FORMAT_MESSAGE_MAX_WIDTH_MASK;
    DWORD fmt = FormatMessageA(flags, NULL, (DWORD)code, 0UL, msg,
        BAL_MAXERROR, NULL);

    assert(0UL != fmt);
    if (fmt > 0UL) {
        if (msg[fmt - 1] == '\n' || msg[fmt - 1] == ' ')
            msg[fmt - 1] = '\0';
    }
#else
    if (gai) {
        const char* tmp = gai_strerror(code);
        _bal_strcpy(msg, BAL_MAXERROR, tmp, strnlen(tmp, BAL_MAXERROR));
    } else {
        int finderr = -1;
# if defined(__HAVE_XSI_STRERROR_R__)
        finderr = strerror_r(code, msg, BAL_MAXERROR);
#  if defined(__HAVE_XSI_STRERROR_R_ERRNO__)
        if (finderr == -1)
            finderr = errno;
#  endif
# elif defined(__HAVE_GNU_STRERROR_R__)
        const char* tmp = strerror_r(code, msg, BAL_MAXERROR);
        if (tmp != msg)
            _bal_strcpy(msg, BAL_MAXERROR, tmp, strnlen(tmp, BAL_MAXERROR));
# elif defined(__HAVE_STRERROR_S__)
        finderr = (int)strerror_s(msg, BAL_MAXERROR, code);
# else
        const char* tmp = strerror(code);
        _bal_strcpy(msg, BAL_MAXERROR, tmp, strnlen(tmp, BAL_MAXERROR));
# endif
# if defined(__HAVE_XSI_STRERROR_R__) || defined(__HAVE_STRERROR_S__)
        BAL_ASSERT_UNUSED(finderr, 0 == finderr);
# else
        BAL_UNUSED(finderr);
# endif
    }
#endif

    __bal_set_os_error(code, msg, func, file, line);
    return false;
}

#if defined(BAL_DBGLOG)
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
# if defined(__WIN__)
        if (NULL != StrStrIA(buf, "error") || NULL != StrStrIA(buf, "assert"))
            color = "91";
        else if (NULL != StrStrIA(buf, "warn"))
            color = "33";
# else
        if (NULL != strcasestr(buf, "error") || NULL != strcasestr(buf, "assert"))
            color = "91";
        else if (NULL != strcasestr(buf, "warn"))
            color = "33";
# endif
# if defined(BAL_DBGLOG_WARNERR_ONLY)
        /* if this macro is defined, only log warnings and errors. */
        if (strncmp("0", color, 2) < 0)
# endif
            (void)printf("\x1b[%sm%s%s\x1b[0m\n", color, prefix, buf);

        _bal_safefree(&buf);
    }
}
#endif
