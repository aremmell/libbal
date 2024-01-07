/*
 * tests.h
 *
 * Author:    Ryan M. Lederman <lederman@gmail.com>
 * Copyright: Copyright (c) 2004-2024
 * Version:   0.3.0
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
#ifndef _BAL_TESTS_H_INCLUDED
# define _BAL_TESTS_H_INCLUDED
# include "tests_shared.h"

/**
 * Test definitions
 */

/**
 * @test baltest_init_cleanup_sanity
 * Ensures that libbal behaves correctly under various circumstances with regards
 * to order and number of init/cleanup operations.
 */
bool baltest_init_cleanup_sanity(void);

/**
 * @test baltest_create_bind_listen_tcp
 * Ensure that a bal_socket can be instantiated, bound to a local address, and
 * set to listen for connections (then be closed and destroyed).
 */
bool baltest_create_bind_listen_tcp(void);

/**
 * @test baltest_error_sanity
 * Ensures that libbal properly handles reported errors and returns the expected
 * error codes and messages for each known error code.
 */
bool baltest_error_sanity(void);

#endif /* !_BAL_TESTS_H_INCLUDED */
