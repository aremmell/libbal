/*
 * tests++.hh
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
#ifndef _BAL_TESTSXX_HH_INCLUDED
# define _BAL_TESTSXX_HH_INCLUDED

# include <bal.hh>
# include <source_location>
# include "tests_shared.h"

/**
 * @addtogroup tests
 * @{
 */

/** libbal C++ wrapper tests. */
namespace bal::tests
{
    /**
     * @test init_with_initializer
     * @brief Ensure the RAII initializer class successfully initializes and
     * cleans up libbal in its constructor and destructor, respectively.
     * @returns true if the test succeeded, false otherwise.
     */
    bool init_with_initializer();

    /**
     * @test raii_socket_sanity
     * @brief Ensure that the RAII socket class is functioning properly.
     * @returns true if the test succeeded, false otherwise.
     */
    bool raii_socket_sanity();

    /**
     * @ test
     * @ brief
     * @ returns true if the test succeeded, false otherwise.
     */
    //bool ();
} // !namespace bal::tests

/** @} */

/** Begins a test by declaring the `pass` variable and entering a try/catch block
 * where an initializer is declared, automatically initializing and cleaning up
 * libbal, even if exceptions are thrown. */
# define _BAL_TEST_COMMENCE \
    bool pass = true; \
    try { \
        initializer balinit;

/** Implements recovery in the event that an unexpected exception is caught. */
# define _BAL_TEST_ON_EXCEPTION(what) \
    std::source_location loc = std::source_location::current(); \
    ERROR_MSG("unexpected exception in %s: '%s'", loc.function_name(), what); \
    pass = false

/** Handles an expected exception. */
# define _BAL_TEST_ON_EXPECTED_EXCEPTION(what) \
    std::source_location loc = std::source_location::current(); \
    TEST_MSG(GREEN("expected exception in %s: '%s'"), loc.function_name(), what)

# define _BAL_TEST_CONCLUDE \
    } catch (bal::exception& ex) { \
        _BAL_TEST_ON_EXCEPTION(ex.what()); \
    } catch (...) { \
        _BAL_TEST_ON_EXCEPTION(BAL_UNKNOWN); \
    } \
    return PRINT_RESULT_RETURN(pass);

#endif // !_BAL_TESTSXX_HH_INCLUDED
