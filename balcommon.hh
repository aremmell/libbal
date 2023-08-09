/*
 * balcommon.hh
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
#ifndef _BAL_COMMON_HH_INCLUDED
# define _BAL_COMMON_HH_INCLUDED

# include "bal.h"
# include <atomic>

# if !defined(__WIN__)
#  include <signal.h>
# endif

namespace balcommon
{
    constexpr const char* localaddr = "127.0.0.1";
    constexpr const char* portnum = "9000";

    bool initialize();
    void quit();
    bool should_run();
    bool install_ctrl_c_handler();
    void ctrl_c_handler_impl();
    void print_last_lib_error(const bal_socket* s = nullptr,
        const char* func = nullptr);

# define EXIT_IF_FAILED(retval, s, func) \
    if (BAL_TRUE != retval) { \
        balcommon::print_last_lib_error(s, func); \
        return EXIT_FAILURE; \
    }

# if defined(__WIN__)
    BOOL WINAPI on_ctrl_c(DWORD ctl_type);
# else
    void on_ctrl_c(int sig);
# endif
} // !namespace balcommon

#endif // !_BAL_COMMON_HH_INCLUDED
