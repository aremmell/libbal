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

    ret = bal_setreuseaddr(&s, 1);
    EXIT_IF_FAILED(ret, nullptr, "bal_setreuseaddr");

    ret = bal_bind(&s, balcommon::localaddr, balcommon::portnum);
    EXIT_IF_FAILED(ret, nullptr, "bal_bind");

    ret = bal_asyncselect(&s, &balserver::async_events_cb, BAL_E_ALL);
    EXIT_IF_FAILED(ret, nullptr, "bal_asyncselect");

    ret = bal_listen(&s, 0);
    EXIT_IF_FAILED(ret, nullptr, "bal_listen");

    printf("listening on %s:%s; ctrl+c to exit...\n",
        balcommon::localaddr, balcommon::portnum);

    do {
        bal_yield_thread();
    } while (balcommon::should_run());

    ret = bal_close(&s);
    EXIT_IF_FAILED(ret, nullptr, "bal_close");

    ret = bal_cleanup();
    EXIT_IF_FAILED(ret, nullptr, "bal_cleanup");

    return EXIT_SUCCESS;
}

void balserver::async_events_cb(bal_socket* s, uint32_t events)
{
    if (bal_isbitset(events, BAL_E_ACCEPT))
    {
        bal_socket client_socket {};
        bal_sockaddr client_addr {};

        int ret = bal_accept(s, &client_socket, &client_addr);
        if (BAL_TRUE != ret) {
            balcommon::print_last_lib_error(nullptr, "bal_accept");
            return;
        }

        bal_addrstrings client_strings {};
        ret = bal_getaddrstrings(&client_addr, 0, &client_strings);
        if (BAL_TRUE != ret) {
            balcommon::print_last_lib_error(nullptr, "bal_getaddrstrings");
            bal_close(&client_socket);
            return;
        }

        ret = bal_asyncselect(&client_socket, &async_events_cb, BAL_E_ALL);
        if (BAL_TRUE != ret) {
            balcommon::print_last_lib_error(nullptr, "bal_asyncselect");
            bal_close(&client_socket);
            return;
        }

        printf("[" BAL_SOCKET_SPEC "] got connection from %s %s:%s; sd = "
               BAL_SOCKET_SPEC "\n", s->sd, client_strings.type, client_strings.ip,
               client_strings.port, client_socket.sd);
    }

    if (bal_isbitset(events, BAL_E_READ))
    {
        static bool sent_reply = false;
        static const char* reply = "O, HELO 2 U";

        char read_buf[2048] = {0};
        int read = bal_recv(s, &read_buf[0], 2047, 0);
        if (read > 0) {
            sent_reply = false;
            printf("[" BAL_SOCKET_SPEC "] read %d bytes: '%s'\n", s->sd, read, read_buf);

            if (bal_iswritable(s) && !sent_reply) {
                int sent = bal_send(s, reply, 11, 0u);
                if (sent > 0) {
                    sent_reply = true;
                    printf("[" BAL_SOCKET_SPEC "] wrote %d bytes\n", s->sd, 11);
                }
            }
        } else {
            printf("[" BAL_SOCKET_SPEC "] read error %d!\n", s->sd, bal_geterror(s));
        }
    }

    if (bal_isbitset(events, BAL_E_CLOSE)) {
        printf("[" BAL_SOCKET_SPEC "] connection closed.\n", s->sd);
    }

    if (bal_isbitset(events, BAL_E_EXCEPTION)) {
        printf("[" BAL_SOCKET_SPEC "] error: got exception condition! err: %d\n", s->sd,
            bal_geterror(s));
    }
}
