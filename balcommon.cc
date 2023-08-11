/*
 * balcommon.cc
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
#include "balcommon.hh"
#include <cstdio>

using namespace std;

atomic_bool _run;

bool balcommon::initialize()
{
    if (!balcommon::install_ctrl_c_handler())
        return false;

    if (BAL_TRUE != bal_init()) {
        print_last_lib_error(nullptr, "bal_init");
        return false;
    }

    _run.store(true);

    return true;
}

void balcommon::quit()
{
    _run.store(false);
}

bool balcommon::should_run()
{
    return _run.load();
}

bool balcommon::install_ctrl_c_handler()
{
#if defined(__WIN__)
    BOOL ret = SetConsoleCtrlHandler(&on_ctrl_c, TRUE);
    assert(FALSE != ret);
    return FALSE != ret;
#else
    struct sigaction sa;
    sa.sa_flags   = 0;
    sa.sa_mask    = {0};
    sa.sa_handler = &on_ctrl_c;

    int ret = sigaction(SIGINT, &sa, nullptr);
    assert(0 == ret);
    return 0 == ret;
#endif
}

void balcommon::ctrl_c_handler_impl()
{
    printf("got ctrl+c; exiting...\n");
    quit();
}

void balcommon::print_last_lib_error(const bal_socket* s /* = nullptr */,
    const char* func /* = nullptr */)
{
    bal_error err = {0, {0}};
    int get = bal_lastsockerror(s, &err);
    assert(BAL_TRUE == get);

    fprintf(stderr, "libbal error: %s %d (%s)\n", (func ? func : ""), err.code, err.desc);
}

#if defined(__WIN__)
BOOL WINAPI balcommon::on_ctrl_c(DWORD ctl_type)
{
    BAL_UNUSED(ctl_type);
    ctrl_c_handler_impl();
    return TRUE;
}
#else
void balcommon::on_ctrl_c(int sig)
{
    BAL_UNUSED(sig);
    ctrl_c_handler_impl();
}
#endif
