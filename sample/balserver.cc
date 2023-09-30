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
#include <bit>

using namespace std;
using namespace bal;
using namespace bal::balcommon;

static balserver::client_map _clients;

int main(int argc, char** argv)
{
    BAL_UNUSED(argc);
    BAL_UNUSED(argv);

    print_startup_banner("balserver");

    if (!initialize()) {
        return EXIT_FAILURE;
    }

    bal_socket* s = nullptr;
    bool ret = bal_create(&s, AF_INET, SOCK_STREAM, IPPROTO_TCP);
    EXIT_IF_FAILED(ret, "bal_create");

    ret = bal_set_reuseaddr(s, 1);
    EXIT_IF_FAILED(ret, "bal_set_reuseaddr");

    ret = bal_bindall(s, portnum);
    EXIT_IF_FAILED(ret, "bal_bindall");

    ret = bal_async_poll(s, &balserver::async_events_cb, BAL_EVT_NORMAL);
    EXIT_IF_FAILED(ret, "bal_async_poll");

    ret = bal_listen(s, SOMAXCONN);
    EXIT_IF_FAILED(ret, "bal_listen");

    printf("listening on %s; ctrl+c to exit...\n", portnum);

    do {
        bal_sleep_msec(sleepfor);
        bal_thread_yield();
    } while (should_run());

    ret = bal_async_poll(s, nullptr, 0U);
    EXIT_IF_FAILED(ret, "bal_async_poll");

    if (!bal_close(&s, true)) {
        print_last_lib_error("bal_close");
    }

    _clients.clear();

    if (!bal_cleanup()) {
        print_last_lib_error("bal_cleanup");
    }

    return EXIT_SUCCESS;
}

balserver::client* get_existing_client(bal_descriptor sd)
{
    if (const auto it = _clients.find(sd); it != _clients.end()) {
        return &it->second;
    }

    return nullptr;
}

void balserver::rem_existing_client(bal_descriptor sd)
{
    if (_clients.erase(sd) > 0)
        PRINT("now have %zu client(s)", _clients.size());
}

void balserver::on_client_connect(bal_socket* s)
{
    bal_socket* client_socket = nullptr;
    bal_sockaddr client_addr {};

    if (!bal_accept(s, &client_socket, &client_addr)) {
        print_last_lib_error("bal_accept");
        return;
    }

    if (!bal_async_poll(client_socket, &async_events_cb, BAL_EVT_NORMAL)) {
        print_last_lib_error("bal_async_poll");
        bal_close(&client_socket, true);
        return;
    }

    client c {client_socket, client_addr};
    address_info addrinfo = c.get_address_info();
    _clients[client_socket->sd] = std::move(c);

    PRINT_SD("got connection from %s %s:%s: " BAL_SOCKET_SPEC " (%" PRIxPTR ");"
        " now have %zu client(s)", s->sd, addrinfo.get_type().c_str(),
        addrinfo.get_addr().c_str(), addrinfo.get_port().c_str(), client_socket->sd,
        std::bit_cast<uintptr_t>(client_socket), _clients.size());
}

void balserver::on_client_disconnect(bal_socket* client_socket, bool error)
{
    if (error) {
        PRINT_SD("connection closed w/ error %d!", client_socket->sd,
            bal_sock_get_error(client_socket));
    } else {
        PRINT_SD("connection closed.", client_socket->sd);
    }

    rem_existing_client(client_socket->sd);
}

void balserver::async_events_cb(bal_socket* s, uint32_t events)
{
    if (bal_isbitset(events, BAL_EVT_ACCEPT)) {
        on_client_connect(s);
    }

    if (bal_isbitset(events, BAL_EVT_READ)) {
        constexpr const size_t buf_size = 2048;
        std::array<char, buf_size> buf {};

        ssize_t read = bal_recv(s, buf.data(), buf.size() - 1, 0);
        if (read > 0) {
            PRINT_SD("read %ld bytes: '%s'", s->sd, read, buf.data());
           bal_addtomask(s, BAL_EVT_WRITE);
        } else if (-1 == read) {
            bal_error err {};
            PRINT_SD("read error %d (%s)!", s->sd, bal_get_error(&err), err.message);
        } else {
            PRINT_SD("read EOF", s->sd);
        }
    }

    if (bal_isbitset(events, BAL_EVT_WRITE)) {
        static const char* reply = "O, HELO 2 U";
        constexpr const size_t reply_size = 11;

        if (ssize_t sent = bal_send(s, reply, reply_size, MSG_NOSIGNAL); sent > 0) {
            PRINT_SD("wrote %ld bytes", s->sd, sent);
        } else {
            bal_error err {};
            PRINT_SD("write error %d (%s)!", s->sd, bal_get_error(&err), err.message);
        }
        bal_remfrommask(s, BAL_EVT_WRITE);
    }

    if (bal_isbitset(events, BAL_EVT_CLOSE)) {
        on_client_disconnect(s, false);
        return;
    }

    if (bal_isbitset(events, BAL_EVT_PRIORITY)) {
        PRINT_SD("priority exceptional condition!", s->sd);
    }

    if (bal_isbitset(events, BAL_EVT_ERROR)) {
        on_client_disconnect(s, true);
    }
}
