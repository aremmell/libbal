/*
 * balserver.cc
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
#include "balserver.hh"
#include <cstdio>
#include <exception>
#include <array>

using namespace std;

int main(int argc, char** argv)
{
    BAL_UNUSED(argc);
    BAL_UNUSED(argv);

    balcommon::print_startup_banner("balserver");

    if (!balcommon::initialize()) {
        return EXIT_FAILURE;
    }

    bal_socket* s = nullptr;
    bool ret = bal_sock_create(&s, AF_INET, SOCK_STREAM, IPPROTO_TCP);
    EXIT_IF_FAILED(ret, "bal_sock_create");

    ret = bal_set_reuseaddr(s, 1);
    EXIT_IF_FAILED(ret, "bal_set_reuseaddr");

    ret = bal_bindall(s, balcommon::portnum);
    EXIT_IF_FAILED(ret, "bal_bindall");

    ret = bal_async_poll(s, &balserver::async_events_cb, BAL_EVT_NORMAL);
    EXIT_IF_FAILED(ret, "bal_async_poll");

    ret = bal_listen(s, SOMAXCONN);
    EXIT_IF_FAILED(ret, "bal_listen");

    printf("listening on %s; ctrl+c to exit...\n", balcommon::portnum);

    do {
        bal_sleep_msec(balcommon::sleepfor);
        bal_thread_yield();
    } while (balcommon::should_run());

    ret = bal_async_poll(s, nullptr, 0U);
    EXIT_IF_FAILED(ret, "bal_async_poll");

    if (!bal_close(&s, true)) {
        balcommon::print_last_lib_error("bal_close");
    }

    if (!bal_cleanup()) {
        balcommon::print_last_lib_error("bal_cleanup");
    }

    return EXIT_SUCCESS;
}

void balserver::async_events_cb(bal_socket* s, uint32_t events)
{
    static bool already_replied = false;

    if (bal_isbitset(events, BAL_EVT_ACCEPT)) {
        bal_socket* client_socket = nullptr;
        bal_sockaddr client_addr {};

        bool ret = bal_accept(s, &client_socket, &client_addr);
        if (!ret) {
            balcommon::print_last_lib_error("bal_accept");
            return;
        }

        bal_addrstrings client_strings {};
        ret = bal_get_addrstrings(&client_addr, false, &client_strings);
        if (!ret) {
            balcommon::print_last_lib_error("bal_get_addrstrings");
            bal_close(&client_socket, true);
            return;
        }

        ret = bal_async_poll(client_socket, &async_events_cb, BAL_EVT_NORMAL);
        if (!ret) {
            balcommon::print_last_lib_error("bal_async_poll");
            bal_close(&client_socket, true);
            return;
        }

        printf("[" BAL_SOCKET_SPEC "] got connection from %s %s:%s: "
            BAL_SOCKET_SPEC " (%p)\n", s->sd, client_strings.type,
            client_strings.addr, client_strings.port, client_socket->sd,
            client_socket);
    }

    if (bal_isbitset(events, BAL_EVT_READ)) {
        constexpr const size_t buf_size = 2048;
        std::array<char, buf_size> buf {};

        ssize_t read = bal_recv(s, buf.data(), buf.size() - 1, 0);
        if (read > 0) {
            printf("[" BAL_SOCKET_SPEC "] read %ld bytes: '%s'\n", s->sd, read,
                buf.data());
           bal_addtomask(s, BAL_EVT_WRITE);
        } else if (-1 == read) {
            bal_error err {};
            printf("[" BAL_SOCKET_SPEC "] read error %d (%s)!\n", s->sd,
                bal_get_error(&err), err.message);
        } else {
            printf("[" BAL_SOCKET_SPEC "] read EOF\n", s->sd);
        }
    }

    if (bal_isbitset(events, BAL_EVT_WRITE) && !already_replied) {
        static const char* reply = "O, HELO 2 U";
        constexpr const size_t reply_size = 11;

        ssize_t sent = bal_send(s, reply, reply_size, MSG_NOSIGNAL);
        if (sent > 0) {
            if (reply_size == sent) {
                already_replied = true;
            }
            printf("[" BAL_SOCKET_SPEC "] wrote %ld bytes\n", s->sd, sent);
        } else {
            bal_error err {};
            printf("[" BAL_SOCKET_SPEC "] write error %d (%s)!\n", s->sd,
                bal_get_error(&err), err.message);
        }
        bal_remfrommask(s, BAL_EVT_WRITE);
    }

    if (bal_isbitset(events, BAL_EVT_CLOSE)) {
        printf("[" BAL_SOCKET_SPEC "] connection closed.\n", s->sd);
        bal_close(&s, true);
        already_replied = false;
        return;
    }

    if (bal_isbitset(events, BAL_EVT_PRIORITY)) {
        printf("[" BAL_SOCKET_SPEC "] priority exceptional condition!\n", s->sd);
    }

    if (bal_isbitset(events, BAL_EVT_ERROR)) {
        printf("[" BAL_SOCKET_SPEC "] ERROR %d!\n", s->sd, bal_sock_get_error(s));
        bal_close(&s, true);
    }
}
