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
using namespace bal;
using namespace bal::common;

int main(int argc, char** argv)
{
    BAL_UNUSED(argc);
    BAL_UNUSED(argv);

    print_startup_banner("balclient");

    if (!initialize()) {
        return EXIT_FAILURE;
    }

    bal_socket* s = nullptr;
    bool ret = bal_create(&s, AF_INET, SOCK_STREAM, IPPROTO_TCP);
    EXIT_IF_FAILED(ret, "bal_create");

    string remote_host = get_input_line("Enter server hostname", localaddr);

    ret = bal_async_poll(s, &balclient::async_events_cb, BAL_EVT_CLIENT);
    EXIT_IF_FAILED(ret, "bal_async_poll");

    printf("connecting to %s:%s...\n", remote_host.c_str(), portnum);

    ret = bal_connect(s, remote_host.c_str(), portnum);
    EXIT_IF_FAILED(ret, "bal_connect");

    printf("running; ctrl+c to exit...\n");

    do {
        bal_sleep_msec(sleepfor);
        bal_thread_yield();
    } while (should_run());

    if (!bal_close(&s, true)) {
        print_last_lib_error("bal_close");
    }

    if (!bal_cleanup()) {
        print_last_lib_error("bal_cleanup");
    }

    return EXIT_SUCCESS;
}

void balclient::async_events_cb(bal_socket* s, uint32_t events)
{
    if (bal_isbitset(events, BAL_EVT_CONNECT)) {
        bal_addrstrings peer_strs {};
        bal_get_peer_strings(s, false, &peer_strs);
        PRINT_SD("connected to %s:%s", s->sd, peer_strs.addr, peer_strs.port);
        bal_addtomask(s, BAL_EVT_WRITE);
    }

    if (bal_isbitset(events, BAL_EVT_CONNFAIL)) {
        bal_error err {};
        bal_get_error(&err);
        PRINT_SD("connection failed! error: %s", s->sd, err.message);
        quit();
    }

    if (bal_isbitset(events, BAL_EVT_READ)) {
        constexpr size_t buf_size = 2048;
        std::array<char, buf_size> buf {};
        ssize_t read = bal_recv(s, buf.data(), buf.size() - 1, 0);
        if (read > 0) {
            PRINT_SD("read %ld bytes: '%s'", s->sd, read, buf.data());
        } else if (-1 == read) {
            bal_error err {};
            PRINT_SD("read error %d (%s)!", s->sd, bal_get_error(&err), err.message);
        } else {
            PRINT_SD("read EOF", s->sd);
        }
    }

    if (bal_isbitset(events, BAL_EVT_WRITE)) {
        static bool wrote_helo = false;
        if (!wrote_helo) {
            const char* req = "HELO";
            constexpr const size_t req_size = 4;

            ssize_t ret = bal_send(s, req, req_size, MSG_NOSIGNAL);
            if (ret <= 0) {
                bal_error err {};
                PRINT_SD("write error %d (%s)!", s->sd, bal_get_error(&err), err.message);
            } else {
                PRINT_SD("wrote %ld bytes", s->sd, ret);
                wrote_helo = true;
                bal_remfrommask(s, BAL_EVT_WRITE);
            }
        }
    }

    if (bal_isbitset(events, BAL_EVT_CLOSE)) {
        PRINT_SD("connection closed.", s->sd);
        quit();
    }

    if (bal_isbitset(events, BAL_EVT_PRIORITY)) {
        PRINT_SD("priority exceptional condition!", s->sd);
    }

    if (bal_isbitset(events, BAL_EVT_ERROR)) {
        PRINT_SD("ERROR %d!", s->sd, bal_get_sock_error(s));
    }
}
