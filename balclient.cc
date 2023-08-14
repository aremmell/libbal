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
#include <array>
#include "balclient.hh"

using namespace std;

int main(int argc, char** argv)
{
    BAL_UNUSED(argc);
    BAL_UNUSED(argv);

    if (!balcommon::initialize())
        return EXIT_FAILURE;

    bal_socket s {};
    int ret = bal_sock_create(&s, AF_INET, IPPROTO_TCP, SOCK_STREAM);
    EXIT_IF_FAILED(ret, nullptr, "bal_sock_create");

    ret = bal_asyncselect(&s, &balclient::async_events_cb, BAL_E_ALL);
    EXIT_IF_FAILED(ret, &s, "bal_asyncselect");

    ret = bal_connect(&s, balcommon::localaddr, balcommon::portnum);
    EXIT_IF_FAILED(ret, &s, "bal_connect");

    printf("running; ctrl+c to exit...\n");

    do {
        bal_yield_thread();
    } while (balcommon::should_run());

    if (BAL_TRUE != bal_close(&s))
        balcommon::print_last_lib_error(&s, "bal_close");

    if (!bal_cleanup())
        balcommon::print_last_lib_error(nullptr, "bal_cleanup");

    return EXIT_SUCCESS;
}

void balclient::async_events_cb(bal_socket* s, uint32_t events)
{
    if (bal_isbitset(events, BAL_E_CONNECT)) {
        printf("[" BAL_SOCKET_SPEC "] connected to %s:%s\n", s->sd, balcommon::localaddr,
            balcommon::portnum);
    }

    if (bal_isbitset(events, BAL_E_CONNFAIL)) {
        bal_error err;
        bal_getlasterror(s, &err);
        fprintf(stderr, "[" BAL_SOCKET_SPEC "] error: failed to connect to %s:%s %d (%s)\n",
            s->sd, balcommon::localaddr, balcommon::portnum, err.code, err.desc);
    }

    if (bal_isbitset(events, BAL_E_READ)) {
        constexpr const size_t buf_size = 2048;
        std::array<char, buf_size> buf;
        int read = bal_recv(s, buf.data(), buf.size() - 1, 0);
        if (read > 0) {
            printf("[" BAL_SOCKET_SPEC "] read %d bytes: '%s'\n", s->sd, read, buf.data());
        } else {
            bal_error err {};
            printf("[" BAL_SOCKET_SPEC "] read error %d (%s)!\n", s->sd,
                bal_getlasterror(s, &err), err.desc);
        }
    }

    if (bal_isbitset(events, BAL_E_WRITE)) {
        static bool wrote_helo = false;

        int err = bal_geterror(s);
        if (0 != err) {
            bal_error err {};
            printf("[" BAL_SOCKET_SPEC "] write error %d (%s)!\n", s->sd,
                bal_getlasterror(s, &err), err.desc);
        } else {
            if (!wrote_helo) {
                const char* req = "HELO";
                constexpr const size_t req_size = 4;

                int ret = bal_send(s, req, req_size, 0U);
                if (ret <= 0)
                    balcommon::print_last_lib_error(s, "bal_send");
                else
                    printf("[" BAL_SOCKET_SPEC "] wrote %d bytes\n", s->sd, ret);
                wrote_helo = true;
            }
        }
    }

    if (bal_isbitset(events, BAL_E_CLOSE)) {
        printf("[" BAL_SOCKET_SPEC "] connection closed.\n", s->sd);
        balcommon::quit();
    }

    if (bal_isbitset(events, BAL_E_EXCEPTION)) {
        printf("[" BAL_SOCKET_SPEC "] error: got exception! err: %d\n", s->sd,
            bal_geterror(s));
        return;
    }
}

/*
    cout << "linked with libbal " << bal_get_versionstring() << " 0x"
    << setw(8) << setbase(16) << setfill('0') << bal_get_versionhex() << endl;
*/
