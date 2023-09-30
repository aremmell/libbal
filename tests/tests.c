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

#pragma message("TODO: implement CLI")
#pragma message("TODO: implement offline-only test runs")

static bal_test_data bal_tests[] = {
    {"init-cleanup-sanity", baltest_init_cleanup_sanity, false, true, false},
    {"create-bind-listen",  baltest_create_bind_listen_tcp, false, true, false},
    {"error-sanity",        baltest_error_sanity, false, true, false}
};

int main(int argc, char** argv)
{
    BAL_UNUSED(argc);
    BAL_UNUSED(argv);

    _bal_tests_init();

    size_t tests_total  = _bal_countof(bal_tests);
    size_t tests_run    = 0;
    size_t tests_passed = 0;

    _bal_start_all_tests(tests_total);

    for (size_t n = 0; n < tests_total; n++) {
        _bal_start_test(tests_total, tests_run, bal_tests[n].name);
        bal_tests[n].pass = bal_tests[n].func();
        _bal_end_test(tests_total, tests_run, bal_tests[n].name, bal_tests[n].pass);
        if (bal_tests[n].pass)
            tests_passed++;
        tests_run++;
    }

    _bal_end_all_tests(tests_total, tests_run, tests_passed);
    return tests_passed == tests_run ? EXIT_SUCCESS : EXIT_FAILURE;
}

/******************************************************************************\
 *                            Test Implementations                            *
\******************************************************************************/

bool baltest_init_cleanup_sanity(void)
{
    bool pass = true;

    /* initialize twice. first should succeed, second should fail. */
    TEST_MSG_0("running bal_init twice in a row...");
    _bal_eqland(pass, bal_init());
    _bal_print_err(pass, false);

    _bal_eqland(pass, !bal_init());
    _bal_print_err(pass, false);

    /* clean up twice. same scenario. */
    TEST_MSG_0("running bal_cleanup twice in a row...");
    _bal_eqland(pass, bal_cleanup());
    _bal_print_err(pass, false);

    _bal_eqland(pass, !bal_cleanup());
    _bal_print_err(pass, false);

    /* initialize after cleanup should succeed. */
    TEST_MSG_0("running bal_init after bal_cleanup...");
    _bal_eqland(pass, bal_init());
    _bal_print_err(pass, false);

    TEST_MSG_0("checking bal_isinitialized...");
    _bal_eqland(pass, bal_isinitialized());
    _bal_print_err(pass, false);

    /* cleanup after init should succeed. */
    TEST_MSG_0("running bal_cleanup after bal_init...");
    _bal_eqland(pass, bal_cleanup());
    _bal_print_err(pass, false);

    return pass;
}

bool baltest_create_bind_listen_tcp(void)
{
    bal_socket* s = NULL;
#pragma message("TODO: create macros that init/clean up")
    TEST_MSG_0("initializing library...");
    bool pass     = bal_init();
    _bal_print_err(pass, false);

    TEST_MSG_0("creating socket...");
    pass = bal_create(&s, AF_INET, SOCK_STREAM, IPPROTO_TCP);
    _bal_print_err(pass, false);

    TEST_MSG_0("binding on all available adapters on port 6969...");
    _bal_eqland(pass, bal_bindall(s, "6969"));
    _bal_print_err(pass, false);

    TEST_MSG_0("registering for async I/O...");
    _bal_eqland(pass, bal_async_poll(s, &_bal_async_poll_callback, BAL_EVT_NORMAL));
    _bal_print_err(pass, false);

    TEST_MSG_0("asynchronously listening for connect events...");
    _bal_eqland(pass, bal_listen(s, SOMAXCONN));
    _bal_print_err(pass, false);

    TEST_MSG_0("closing and destroying socket...");
    _bal_eqland(pass, bal_close(&s, true));
    _bal_print_err(pass, false);

    TEST_MSG_0("cleaning up library...");
    _bal_eqland(pass, bal_cleanup());
    _bal_print_err(pass, false);

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
        {BAL_E_ASNOTINIT,  "BAL_E_ASNOTINIT"},  /* Asynchronous I/O is not initialized */
        {BAL_E_ASDUPEINIT, "BAL_E_ASDUPEINIT"}, /* Asynchronous I/O is already initialized */
        {BAL_E_ASNOSOCKET, "BAL_E_ASNOSOCKET"}, /* Socket is not registered for asynchronous I/O events */
        {BAL_E_BADEVTMASK, "BAL_E_BADEVTMASK"}, /* Invalid asynchronous I/O event bitmask */
        {BAL_E_INTERNAL,   "BAL_E_INTERNAL"},   /* An internal error has occurred */
        {BAL_E_UNAVAIL,    "BAL_E_UNAVAIL"},    /* Feature is disabled or unavailable */
        {BAL_E_PLATFORM,   "BAL_E_PLATFORM"},   /* Platform error code %d: %s */
        {BAL_E_UNKNOWN,    "BAL_E_UNKNOWN"}     /* An unknown error has occurred */
    };

    /* for BAL_E_PLATFORM, there should be an error set. */
#if defined(__WIN__)
        _bal_handleerr(WSAENOTSOCK);
#else
        _bal_handleerr(ENOTSOCK);
#endif

    bool repeat = false;
    for (size_t n = 0; n < _bal_countof(error_dict); n++) {
        (void)_bal_seterror(_bal_mk_error(error_dict[n].code));
        bal_error err = {0};

        /* without extended information. */
        int ret = bal_get_error(&err);
        _bal_eqland(pass, error_dict[n].code == ret && ret == err.code);
        _bal_eqland(pass, err.message[0] != '\0');
        TEST_MSG("%s = %s", error_dict[n].as_string, err.message);

        /* with extended information. */
        ret = bal_get_error_ext(&err);
        _bal_eqland(pass, error_dict[n].code == ret && ret == err.code);
        _bal_eqland(pass, err.message[0] != '\0');
        TEST_MSG("%s [ext] = %s", error_dict[n].as_string, err.message);

        /* getaddrinfo/getnameinfo errors. */
        if (BAL_E_PLATFORM == error_dict[n].code && !repeat) {
            _bal_handlegaierr(EAI_SERVICE);
            repeat = true;
            n--;
        }
    }

    return pass;
}
