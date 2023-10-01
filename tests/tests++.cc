/*
 * tests++.cc
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
#include "tests++.hh"
#include <vector>
#include <cstdlib>

using namespace bal;

static std::vector<bal_test_data> bal_tests = {
    {"raii-initializer",   tests::init_with_initializer, false, true, false},
    {"raii_socket_sanity", tests::raii_socket_sanity, false, true, false }
};

int main(int argc, char** argv)
{
    BAL_UNUSED(argc);
    BAL_UNUSED(argv);

    _bal_tests_init();

    size_t tests_total  = bal_tests.size();
    size_t tests_run    = 0;
    size_t tests_passed = 0;

    _bal_start_all_tests(tests_total);

    for (size_t n = 0; n < tests_total; n++) {
        _bal_start_test(tests_total, tests_run, bal_tests[n].name);
        bal_tests[n].pass = bal_tests[n].func();
        _bal_end_test(tests_total, tests_run, bal_tests[n].name, bal_tests[n].pass);
        if (bal_tests[n].pass)
            tests_passed++;
        tests_run++;
    }

    _bal_end_all_tests(tests_total, tests_run, tests_passed);
    return tests_passed == tests_run ? EXIT_SUCCESS : EXIT_FAILURE;
}

/**
 * Test implementations
 */

bool bal::tests::init_with_initializer()
{
    /* can't use _BAL_TEST_COMMENCE, since it declares an initializer. */
    bool pass = true;
    try {

    /* scope an initializer and ensure that libbal is initialized by its ctor,
     * and cleaned up by its dtor. */
    {
        TEST_MSG_0("create a scoped initializer to initialize libbal...");
        initializer initbal; /* will throw if bal_init() fails. */

        TEST_MSG_0("created; test bal_isinitialized()...");
        _bal_eqland(pass, bal_isinitialized());

        TEST_MSG_0("allow initializer to be destructed, cleaning up libbal...");
    }

    TEST_MSG_0("initializer destructed; test bal_isinitialized()...");
    _bal_eqland(pass, !bal_isinitialized());

    _BAL_TEST_CONCLUDE
}

bool bal::tests::raii_socket_sanity()
{
    _BAL_TEST_COMMENCE

    constexpr const char* port_num = "9969";

    /* scope an RAII socket and ensure that it is valid and can be used in
     * Berkeley socket calls, such as bind(). */
    bal_descriptor sd = 0;
    {
        TEST_MSG_0("create a scoped socket...");
        scoped_socket sock(AF_INET, SOCK_STREAM, IPPROTO_TCP);

        TEST_MSG("created; testing bal_bindall(), port %s...", port_num);
        _bal_eqland(pass, sock.is_valid());

        // TODO: replace with socket_base::bind_all
        _bal_eqland(pass, bal_bindall(sock.get(), port_num));

        TEST_MSG_0("bound; allow socket to be destructed...");
        sd = sock.get()->sd;
    }

    TEST_MSG_0("socket_destructed; ensure socket closed/destroyed...");
#if defined(__WIN__)
    _bal_eqland(pass, SOCKET_ERROR == listen(sd, SOMAXCONN));
    _bal_eqland(pass, WSAGetLastError() == WSAENOTSOCK);
#else
    _bal_eqland(pass, -1 == listen(sd, SOMAXCONN));
    _bal_eqland(pass, errno == ENOTSOCK || errno == EBADF);
#endif

    _BAL_TEST_CONCLUDE
}

/*bool bal::tests::()
{
    _BAL_TEST_COMMENCE

    _BAL_TEST_CONCLUDE
}*/
