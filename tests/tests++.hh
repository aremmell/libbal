/*
 * tests++.hh
 *
 * Author:    Ryan M. Lederman <lederman@gmail.com>
 * Copyright: Copyright (c) 2004-2025
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
#ifndef _BAL_TESTSXX_HH_INCLUDED
# define _BAL_TESTSXX_HH_INCLUDED

# include <bal.hh>
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

/** Emits an error message when an unexpected exception is caught. */
# define _BAL_TEST_LOG_UNEXPECTED_EXCEPTION(func, what) \
     ERROR_MSG("unexpected exception in %s: '%s'", (func), (what)); \
     pass = false

/** Emits a message when an expected exception is caught. */
# define _BAL_TEST_LOG_EXPECTED_EXCEPTION(func, what) \
    TEST_MSG(GREEN("expected exception in %s: '%s'"), (func), (what))

/** Implements recovery in the event that an unexpected exception is caught. */
# if defined(source_location)
#  define _BAL_TEST_ON_EXCEPTION(ex) \
     source_location loc = source_location::current(); \
     _BAL_TEST_LOG_UNEXPECTED_EXCEPTION(loc.function_name(), ex.what())
# else
#  define _BAL_TEST_ON_EXCEPTION(ex) \
    _BAL_TEST_LOG_UNEXPECTED_EXCEPTION(__func__, ex.what())
# endif

/** Handles an expected exception. */
# if defined(source_location)
#  define _BAL_TEST_ON_EXPECTED_EXCEPTION(ex) \
    source_location loc = source_location::current(); \
    _BAL_TEST_LOG_EXPECTED_EXCEPTION(loc.function_name(), ex.what())
# else
#  define _BAL_TEST_ON_EXPECTED_EXCEPTION(ex) \
    _BAL_TEST_LOG_EXPECTED_EXCEPTION(__func__, ex.what())
# endif

# define _BAL_TEST_CONCLUDE \
    } catch (bal::exception& ex) { \
        _BAL_TEST_ON_EXCEPTION(ex); \
    } catch (...) { \
        _BAL_TEST_ON_EXCEPTION(bal::exception(BAL_UNKNOWN)); \
    } \
    return PRINT_RESULT_RETURN(pass);

#endif // !_BAL_TESTSXX_HH_INCLUDED
