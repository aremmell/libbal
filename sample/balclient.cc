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

    try {
        print_startup_banner("balclient");

        if (!initialize()) {
            throw bal::exception("failed to initialize bal::common");
        }

        initializer balinit;
        scoped_socket sock {AF_INET, SOCK_STREAM, IPPROTO_TCP};

        sock.on_connect = [&sock]()
        {
            address peer_addr {};
            sock.get_peer_addr(peer_addr);
            auto addr_info = peer_addr.get_address_info();
            printf("connected to %s:%s", addr_info.get_addr().c_str(),
                addr_info.get_port().c_str());
            sock.want_write_events(true);
        };

        sock.on_conn_fail = [&sock]()
        {
            const auto err = sock.get_error(false);
            PRINT_SD("connection failed! errror: %s", sock.get()->sd,
                err.message.c_str());
            quit();
        };

        sock.on_read = [&sock]()
        {
            constexpr size_t buf_size = 2048;
            std::array<char, buf_size> buf {};
            if (ssize_t read = sock.recv(buf.data(), buf.size() - 1, 0); read > 0) {
                PRINT_SD("read %ld bytes: '%s'", sock.get()->sd, read, buf.data());
            } else if (-1 == read) {
                const auto err = sock.get_error(false);
                PRINT_SD("read error %d (%s)!", sock.get()->sd, err.code,
                    err.message.c_str());
            } else {
                PRINT_SD("read EOF", sock.get()->sd);
            }
        };

        sock.on_write = [&sock]()
        {
            const char* req = "HELO";
            constexpr const size_t req_size = 4;

            ssize_t ret = sock.send(req, req_size, MSG_NOSIGNAL);
            if (ret <= 0) {
                const auto err = sock.get_error(false);
                PRINT_SD("write error %d (%s)!", sock.get()->sd, err.code,
                    err.message.c_str());
            } else {
                PRINT_SD("wrote %ld bytes", sock.get()->sd, ret);
            }

            sock.want_write_events(false);
        };

        sock.on_close = [&sock]()
        {
            PRINT_SD("connection closed.", sock.get()->sd);
            quit();
        };

        sock.async_poll(BAL_EVT_CLIENT);

        string remote_host = get_input_line("Enter server hostname", localaddr);
        printf("connecting to %s:%s...\n", remote_host.c_str(), portnum);

        sock.connect(remote_host, portnum);

        printf("running; ctrl+c to exit...\n");

        do {
            bal_sleep_msec(sleepfor);
            bal_thread_yield();
        } while (should_run());

        return EXIT_SUCCESS;
    } catch (bal::exception& ex) {
        printf("error: caught exception: '%s'!", ex.what());
        return EXIT_FAILURE;
    }
}
