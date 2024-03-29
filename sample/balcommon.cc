/*
 * balcommon.cc
 *
 * Author:    Ryan M. Lederman <lederman@gmail.com>
 * Copyright: Copyright (c) 2004-2024
 * Version:   0.3.0
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
#include <iostream>

using namespace std;

static atomic_bool _run;

bool bal::common::initialize()
{
    if (!install_ctrl_c_handler()) {
        return false;
    }

    _run.store(true);

    return true;
}

void bal::common::quit()
{
    _run.store(false);
}

bool bal::common::should_run()
{
    return _run.load();
}

bool bal::common::install_ctrl_c_handler()
{
#if defined(__WIN__)
    BOOL ret = SetConsoleCtrlHandler(&on_ctrl_c, TRUE);
    BAL_ASSERT(FALSE != ret);
    return FALSE != ret;
#else
    struct sigaction action;
    action.sa_flags   = 0;
    action.sa_mask    = {0};
    action.sa_handler = &on_ctrl_c;

    int ret = sigaction(SIGINT, &action, nullptr);
    BAL_ASSERT(0 == ret);
    return 0 == ret;
#endif
}

void bal::common::ctrl_c_handler_impl()
{
    printf("got ctrl+c; exiting...\n");
    quit();
}

void bal::common::print_startup_banner(const string& name)
{
    cout << name << " (libbal " << bal_get_versionstring() << ")" << endl;
}

string bal::common::get_input_line(const string& prompt, const string& def)
{
    string input;

    cout << prompt << " [" << def << "]: ";
    getline(cin, input);

    if (input.empty())
        input = def;

    return input;
}

#if defined(__WIN__)
BOOL WINAPI bal::common::on_ctrl_c(DWORD ctl_type)
{
    BAL_UNUSED(ctl_type);
    ctrl_c_handler_impl();
    return TRUE;
}
#else
void bal::common::on_ctrl_c(int sig)
{
    BAL_UNUSED(sig);
    ctrl_c_handler_impl();
}
#endif
