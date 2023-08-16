/*
 * balclient.cc
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
#include <iostream>
#include <iomanip>
#include <exception>
#include <array>
#include "balclient.hh"

using namespace std;

int main(int argc, char** argv)
{
    BAL_UNUSED(argc);
    BAL_UNUSED(argv);

    if (!balcommon::initialize()) {
        return EXIT_FAILURE;
    }

    bal_socket* s = nullptr;
    int ret = bal_sock_create(&s, AF_INET, SOCK_STREAM, IPPROTO_TCP);
    EXIT_IF_FAILED(ret, "bal_sock_create");

    ret = bal_asyncselect(s, &balclient::async_events_cb, BAL_E_ALL);
    EXIT_IF_FAILED(ret, "bal_asyncselect");

    ret = bal_connect(s, balcommon::localaddr, balcommon::portnum);
    EXIT_IF_FAILED(ret, "bal_connect");

    printf("running; ctrl+c to exit...\n");

    do {
        bal_sleep_msec(balcommon::sleepfor);
        bal_thread_yield();
    } while (balcommon::should_run());

    if (BAL_TRUE != bal_close(&s, true)) {
        balcommon::print_last_lib_error("bal_close");
    }

    if (!bal_cleanup()) {
        balcommon::print_last_lib_error("bal_cleanup");
    }

    return EXIT_SUCCESS;
}

void balclient::async_events_cb(bal_socket* s, uint32_t events)
{
    if (bal_isbitset(events, BAL_E_CONNECT)) {
        printf("[" BAL_SOCKET_SPEC "] connected to %s:%s\n", s->sd,
            balcommon::localaddr, balcommon::portnum);
        bal_addtomask(s, BAL_E_WRITE);
    }

    if (bal_isbitset(events, BAL_E_CONNFAIL)) {
        bal_error err {};
        printf("[" BAL_SOCKET_SPEC "] failed to connect to %s:%s %d (%s)\n",
            s->sd, balcommon::localaddr, balcommon::portnum,
            bal_getlasterror(&err), err.desc);
        balcommon::quit();
    }

    if (bal_isbitset(events, BAL_E_READ)) {
        constexpr const size_t buf_size = 2048;
        std::array<char, buf_size> buf;
        int read = bal_recv(s, buf.data(), buf.size() - 1, 0);
        if (read > 0) {
            printf("[" BAL_SOCKET_SPEC "] read %d bytes: '%s'\n", s->sd, read,
                buf.data());
            bal_addtomask(s, BAL_E_WRITE);
        } else if (-1 == read) {
            bal_error err {};
            printf("[" BAL_SOCKET_SPEC "] read error %d (%s)!\n", s->sd,
                bal_getlasterror(&err), err.desc);
        } else {
            printf("[" BAL_SOCKET_SPEC "] read EOF\n", s->sd);
        }
    }

    if (bal_isbitset(events, BAL_E_WRITE)) {
        static bool wrote_helo = false;
        if (!wrote_helo) {
            const char* req = "HELO";
            constexpr const size_t req_size = 4;

            int ret = bal_send(s, req, req_size, MSG_NOSIGNAL);
            if (ret <= 0) {
                bal_error err {};
                printf("[" BAL_SOCKET_SPEC "] write error %d (%s)!\n", s->sd,
                    bal_getlasterror(&err), err.desc);
            } else {
                printf("[" BAL_SOCKET_SPEC "] wrote %d bytes\n", s->sd, ret);
                wrote_helo = true;
                bal_remfrommask(s, BAL_E_WRITE);
            }
        }
    }

    if (bal_isbitset(events, BAL_E_CLOSE)) {
        printf("[" BAL_SOCKET_SPEC "] connection closed.\n", s->sd);
        balcommon::quit();
    }

    if (bal_isbitset(events, BAL_E_EXCEPT)) {
        printf("[" BAL_SOCKET_SPEC "] exceptional condition!\n", s->sd);
    }

    if (bal_isbitset(events, BAL_E_ERROR)) {
        printf("[" BAL_SOCKET_SPEC "] ERROR %d!\n", s->sd, bal_geterror(s));
    }
}

/*
    cout << "linked with libbal " << bal_get_versionstring() << " 0x"
    << setw(8) << setbase(16) << setfill('0') << bal_get_versionhex() << endl;
*/
