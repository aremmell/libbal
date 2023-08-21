/*
 * tests.c
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
#include "tests.h"
#include <stdlib.h>

static const bal_test_data bal_tests[] = {
    {"init-cleanup-sanity", baltest_init_cleanup_sanity},
    {"error-sanity",        baltest_error_sanity}
};

int main(int argc, char** argv)
{
    BAL_UNUSED(argc);
    BAL_UNUSED(argv);

    size_t tests_total  = _bal_countof(bal_tests);
    size_t tests_run    = 0UL;
    size_t tests_passed = 0UL;

    printf("running %zu " ULINE("libbal") " tests...\n", tests_total);

    for (size_t n = 0UL; n < tests_total; n++) {
        printf("\n- %s ...\n\n", bal_tests[n].name);
        bool pass = bal_tests[n].func();
        if (pass) {
            printf("\n- %s: PASS\n\n", bal_tests[n].name);
            tests_passed++;
        } else {
            printf("\n- %s: FAIL!\n\n", bal_tests[n].name);
        }
        tests_run++;
    }

    printf("(%zu/%zu) tests passed\n", tests_passed, tests_run);

    return tests_passed == tests_run ? EXIT_SUCCESS : EXIT_FAILURE;
}

/******************************************************************************\
 *                            Test Implementations                            *
\******************************************************************************/

bool baltest_init_cleanup_sanity(void)
{
    bool pass = true;
    printf("NOOP\n");
    return pass;
}

bool baltest_error_sanity(void)
{
    bool pass = true;

    static const struct {
        const int code;
        const char* const as_string;
    } error_dict[] = {
        {BAL_E_NULLPTR,    "BAL_E_NULLPTR"},    /* NULL pointer argument */
        {BAL_E_BADSTRING,  "BAL_E_BADSTRING"},  /* Invalid string argument */
        {BAL_E_BADSOCKET,  "BAL_E_BADSOCKET"},  /* Invalid bal_socket argument */
        {BAL_E_BADBUFLEN,  "BAL_E_BADBUFLEN"},  /* Invalid buffer length argument */
        {BAL_E_INVALIDARG, "BAL_E_INVALIDARG"}, /* Invalid argument */
        {BAL_E_NOTINIT,    "BAL_E_NOTINIT"},    /* libbal is not initialized */
        {BAL_E_DUPEINIT,   "BAL_E_DUPEINIT"},   /* libbal is already initialized */
        {BAL_E_DUPESOCKET, "BAL_E_DUPESOCKET"}, /* Socket is already registered for asynchronous I/O events */
        {BAL_E_NOSOCKET,   "BAL_E_NOSOCKET"},   /* Socket is not registered for asynchronous I/O events */
        {BAL_E_BADEVTMASK, "BAL_E_BADEVTMASK"}, /* Invalid asynchronous I/O event bitmask */
        {BAL_E_INTERNAL,   "BAL_E_INTERNAL"},   /* An internal error has occurred */
        {BAL_E_UNAVAIL,    "BAL_E_UNAVAIL"}     /* Feature is disabled or unavailable */
    };

    /* test the libbal-specific errors. */
    for (size_t n = 0UL; n < _bal_countof(error_dict); n++) {
        _bal_handleerr(_bal_mk_error(error_dict[n].code));
        bal_error err = {0};
        (void)_bal_get_last_error(&err);
        pass &= err.code == error_dict[n].code;
        pass &= err.desc[0] != '\0';
        printf("%s = %s\n", error_dict[n].as_string, err.desc);
    }

    /* test OS-level errors. */
#if defined(__WIN__)
    int os_err = WSAEINTR;
    _bal_handleerr(os_err);
#else
    int os_err = EINTR;
    _bal_handleerr(os_err);
#endif

    bal_error err = {0};
    (void)_bal_get_last_error(&err);
    pass &= err.code == os_err;
    pass &= err.desc[0] != '\0';
    printf("%d = %s\n", os_err, err.desc);

    return pass;
}

/******************************************************************************\
 *                                Test Harness                                *
\******************************************************************************/

/* void _bal_print(int text_attr, int fg_color, int bg_color, const char* format, ...)
{
}

void _bal_infomsg(const char* format, ...)
{
} */
