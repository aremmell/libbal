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
#include "balclient.hh"

using namespace std;

int main(int argc, char** argv)
{
    BAL_UNUSED(argc);
    BAL_UNUSED(argv);

    if (!balcommon::initialize())
        return EXIT_FAILURE;

    bal_socket s;
    int ret = bal_sock_create(&s, AF_INET, IPPROTO_TCP, SOCK_STREAM);
    EXIT_IF_FAILED(ret, nullptr, "bal_sock_create");

    ret = bal_asyncselect(&s, &balclient::async_events_cb, BAL_E_ALL);
    EXIT_IF_FAILED(ret, nullptr, "bal_asyncselect");

    ret = bal_connect(&s, balcommon::localaddr, balcommon::portnum);
    EXIT_IF_FAILED(ret, nullptr, "bal_connect");

    printf("running; ctrl+c to exit...\n");

    do {
        int yield = sched_yield();
        assert(0 == yield);
    } while (balcommon::should_run());

    ret = bal_close(&s);
    EXIT_IF_FAILED(ret, nullptr, "bal_close");

    ret = bal_finalize();
    EXIT_IF_FAILED(ret, nullptr, "bal_finalize");

    return EXIT_SUCCESS;
}

void balclient::async_events_cb(const bal_socket* s, uint32_t events)
{
    if (bal_isbitset(events, BAL_E_CONNECT)) {
        printf("connected to %s:%s\n", balcommon::localaddr,
            balcommon::portnum);
    }

    if (bal_isbitset(events, BAL_E_CONNFAIL)) {
        bal_error err;
        bal_lastsockerror(s, &err);
        fprintf(stderr, "error: failed to connect to %s:%s %d (%s)\n",
            balcommon::localaddr, balcommon::portnum, err.code, err.desc);
    }

    static bool wrote_helo = false;
    if (bal_isbitset(events, BAL_E_WRITE)) {
        int err = bal_geterror(s);
        if (0 != err) {
            printf("write error %d!\n", err);
        } else {
            if (!wrote_helo) {
                const char* req = "HELO";
                int ret = bal_send(s, req, 4, 0u);
                if (BAL_FALSE == ret)
                    balcommon::print_last_lib_error(s, "bal_send");
                wrote_helo = true;
            }
        }
    }

    if (bal_isbitset(events, BAL_E_CLOSE)) {
        printf("connection closed.\n");
        balcommon::quit();
    }

    if (bal_isbitset(events, BAL_E_EXCEPTION)) {
        printf("error: got exception condition! err: %d\n",
            bal_geterror(s));
        return;
    }
}

/*
    cout << "linked with libbal " << bal_get_versionstring() << " 0x"
    << setw(8) << setbase(16) << setfill('0') << bal_get_versionhex() << endl;
*/
