/*
 * balclient.cc
 *
 * Author:    Ryan M. Lederman <lederman@gmail.com>
 * Copyright: Copyright (c) 2004-2025
 * Version:   0.3.1
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
#include <sstream>

static constexpr const char* quit_msg = "quit";
static constexpr const char* helo_msg = "HELO";

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

        string send_buffer = helo_msg;
        initializer balinit;
        scoped_socket main_sock {AF_INET, SOCK_STREAM, IPPROTO_TCP};

        main_sock.on_connect = [](scoped_socket* sock)
        {
            address peer_addr {};
            sock->get_peer_addr(peer_addr);
            auto addr_info = peer_addr.get_address_info();
            PRINT("connected to %s:%s", addr_info.get_addr().c_str(),
                addr_info.get_port().c_str());
            sock->want_write_events(true);
            return true;
        };

        main_sock.on_conn_fail = [](const scoped_socket* sock)
        {
            const auto err = sock->get_error(false);
            PRINT_SD("connection failed! errror: %s", sock->get_descriptor(),
                err.message.c_str());
            quit();
            return false;
        };

        main_sock.on_read = [&sb = send_buffer](scoped_socket* sock)
        {
            std::array<char, read_buf_size> buf {};
            if (ssize_t read = sock->recv(buf.data(), buf.size() - 1, 0); read > 0) {
                PRINT_SD("read %ld bytes: '%s'", sock->get_descriptor(), read, buf.data());
                std::stringstream strm {"Enter text to send (or '"};
                strm << quit_msg << "')";
                sb = get_input_line(strm.str(), helo_msg);
                if (_bal_strsame(sb.c_str(), quit_msg, 4)) {
                    sb.clear();
                    quit();
                } else {
                    sock->want_write_events(true);
                }
            } else if (-1 == read) {
                const auto err = sock->get_error(false);
                PRINT_SD("read error %d (%s)!", sock->get_descriptor(), err.code,
                    err.message.c_str());
            }
            return true;
        };

        main_sock.on_write = [&sb = send_buffer](scoped_socket* sock)
        {
            if (!sb.empty()) {
                if (ssize_t sent = sock->send(sb.data(), sb.size(),
                    MSG_NOSIGNAL); sent > 0) {
                    PRINT_SD("wrote %ld bytes", sock->get_descriptor(), sent);
                } else {
                    const auto err = sock->get_error(false);
                    PRINT_SD("write error %d (%s)!", sock->get_descriptor(), err.code,
                        err.message.c_str());
                }
                sb.clear();
            }

            sock->want_write_events(false);
            return true;
        };

        main_sock.on_close = [](const scoped_socket* sock)
        {
            PRINT_SD("connection closed.", sock->get_descriptor());
            quit();
            return false;
        };

        main_sock.on_error = [](const scoped_socket* sock)
        {
            const auto err = sock->get_error(false);
            PRINT_SD("error: %d (%s)!", sock->get_descriptor(), err.code, err.message.c_str());
            quit();
            return false;
        };

        main_sock.async_poll(BAL_EVT_CLIENT);

        string remote_host = get_input_line("Enter server hostname", localaddr);
        PRINT("connecting to %s:%s...", remote_host.c_str(), portnum);

        main_sock.connect(remote_host, portnum);

        PRINT_0("running; ctrl+c to exit...");

        do {
            bal_sleep_msec(sleep_interval);
            bal_thread_yield();
        } while (should_run());

        return EXIT_SUCCESS;
    } catch (bal::exception& ex) {
        PRINT("error: caught exception: '%s'!", ex.what());
        return EXIT_FAILURE;
    }
}
