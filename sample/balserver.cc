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
using namespace bal::common;
using namespace bal::server;

static client_map _clients;

int main(int argc, char** argv)
{
    BAL_UNUSED(argc);
    BAL_UNUSED(argv);

    try {
        print_startup_banner("balserver");

        if (!initialize()) {
            throw bal::exception("failed to initialize bal::common");
        }

        string send_buffer;
        initializer balinit;

        auto client_on_read = [&send_buffer](scoped_socket* sock) -> bool
        {
            constexpr size_t buf_size = 2048;
            std::array<char, buf_size> buf {};

            if (ssize_t read = sock->recv(buf.data(), buf.size() - 1, 0); read > 0) {
                PRINT_SD("read %ld bytes: '%s'", sock->get_descriptor(), read, buf.data());
                send_buffer = "You said '";
                send_buffer += buf.data();
                send_buffer += "'; acknowledged.";
                sock->want_write_events(true);
            } else if (-1 == read) {
                const auto err = sock->get_error(false);
                PRINT_SD("read error %d (%s)!", sock->get_descriptor(), err.code,
                    err.message.c_str());
            } else {
                PRINT_SD("read EOF", sock->get_descriptor());
            }

            return true;
        };

        auto client_on_write = [&send_buffer](scoped_socket* sock) -> bool
        {
            if (!send_buffer.empty()) {
                if (ssize_t sent = sock->send(send_buffer.data(), send_buffer.size(),
                    MSG_NOSIGNAL); sent > 0) {
                    PRINT_SD("wrote %ld bytes", sock->get_descriptor(), sent);
                } else {
                    const auto err = sock->get_error(false);
                    PRINT_SD("write error %d (%s)!", sock->get_descriptor(), err.code,
                        err.message.c_str());
                }
                send_buffer.clear();
            }

            sock->want_write_events(false);
            return true;
        };

        auto client_on_close = [](scoped_socket* sock) -> bool
        {
            on_client_disconnect(sock->get(), false);
            sock->close();
            return false;
        };

        auto client_on_error = [](scoped_socket* sock) -> bool
        {
            on_client_disconnect(sock->get(), true);
            sock->close();
            return false;
        };

        scoped_socket sock {AF_INET, SOCK_STREAM, IPPROTO_TCP};
        sock.on_incoming_conn = [=](scoped_socket* sock) -> bool
        {
            scoped_socket client_sock;
            address client_addr {};
            sock->accept(client_sock, client_addr);

            client_sock.on_read  = client_on_read;
            client_sock.on_write = client_on_write;
            client_sock.on_close = client_on_close;
            client_sock.on_error = client_on_error;

            client_sock.async_poll(BAL_EVT_NORMAL);

            address_info addrinfo = client_addr.get_address_info();
            PRINT("got connection from %s %s:%s: " BAL_SOCKET_SPEC " (0x%" PRIxPTR ");"
                " now have %zu client(s)", addrinfo.get_type().c_str(),
                addrinfo.get_addr().c_str(), addrinfo.get_port().c_str(),
                client_sock.get_descriptor(), std::bit_cast<uintptr_t>(client_sock.get()),
                _clients.size() + 1);

            _clients[client_sock.get_descriptor()] = std::move(client_sock);
            return true;
        };

        sock.set_reuseaddr(1);
        sock.bind_all(portnum);
        sock.async_poll(BAL_EVT_NORMAL);
        sock.listen(SOMAXCONN);

        PRINT("listening on %s; ctrl+c to exit...", portnum);

        do {
            bal_sleep_msec(sleepfor);
            bal_thread_yield();
        } while (should_run());

        if (!_clients.empty()) {
            PRINT("closing/destroying %zu socket(s)...", _clients.size());
            _clients.clear();
        }

        return EXIT_SUCCESS;
    } catch (bal::exception& ex) {
        PRINT("error: caught exception: '%s'!", ex.what());
        return EXIT_FAILURE;
    }
}

scoped_socket* bal::server::get_existing_client(bal_descriptor sd)
{
    if (const auto it = _clients.find(sd); it != _clients.end()) {
        return &it->second;
    }

    return nullptr;
}

void bal::server::rem_existing_client(bal_descriptor sd)
{
    if (_clients.erase(sd) > 0)
        PRINT("now have %zu client(s)", _clients.size());
}

void bal::server::on_client_disconnect(bal_socket* s, bool error)
{
    if (error) {
        const auto code = bal_get_sock_error(s);
        PRINT_SD("connection closed with error: %d!", s->sd, code);
    } else {
        PRINT_SD("connection closed", s->sd);
    }

    rem_existing_client(s->sd);
}
