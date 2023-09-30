/*
 * tests_shared.c
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
#include "tests_shared.h"

void _bal_tests_init(void)
{
#if defined(__WIN__)
    DWORD flags = ENABLE_VIRTUAL_TERMINAL_PROCESSING | ENABLE_PROCESSED_OUTPUT;
    SetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), flags);
    SetConsoleMode(GetStdHandle(STD_ERROR_HANDLE), flags);
#endif
}

void _bal_start_all_tests(size_t total)
{
    printf("\n" WHITEB("" ULINE("libbal") " %s (%s) running %zu %s...") "\n\n",
        bal_get_versionstring(), (bal_is_releasebuild() ? "" : "prerelease"),
        total, _TEST_PLURAL(total));
}

void _bal_start_test(size_t total, size_t run, const char* name)
{
    printf(WHITEB("(%zu/%zu) '%s'...") "\n\n", run + 1, total, name);
}

bool _bal_print_err(bool pass, bool expected)
{
    if (!pass) {
        bal_error err = {0};
        bal_get_error_ext(&err);
        if (BAL_E_NOERROR != err.code) {
            if (expected) {
                TEST_MSG(GREEN("Expected: %d (%s)"), err.code, err.message);
            } else {
                TEST_MSG(RED("!! Unexpected: %d (%s)"), err.code, err.message);
            }
        }
    }
    return pass;
}

void _bal_end_test(size_t total, size_t run, const char* name, bool pass)
{
    if (pass) {
        printf("\n" WHITEB("(%zu/%zu) '%s': ") GREEN("PASS") "\n\n", run + 1, total, name);
    } else {
        printf("\n" WHITEB("(%zu/%zu) '%s': ") RED("FAIL") "\n\n", run + 1, total, name);
    }
}

void _bal_end_all_tests(size_t total, size_t run, size_t passed)
{
    if (run == passed) {
        printf(GREENB("all %zu "  ULINE("libbal") " %s passed!") "\n", run,
            _TEST_PLURAL(run));
    } else {
        printf(REDB("%zu of %zu " ULINE("libbal") " %s failed") "\n", run - passed,
            total, _TEST_PLURAL(run));
    }
}

void _bal_async_poll_callback(bal_socket* s, uint32_t events)
{
#pragma message("TODO: print events")
    BAL_UNUSED(s);
    BAL_UNUSED(events);
}
