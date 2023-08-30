/*
 * balstate.c
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
#include "bal/state.h"

/******************************************************************************\
 *                                   Globals                                  *
\******************************************************************************/

/* one-time initializer for async I/O data container. */
bal_once _bal_static_once_init = BAL_ONCE_INIT;

/* whether or not the async I/O machinery is initialized. */
#if defined(__HAVE_STDATOMICS__)
atomic_bool _bal_async_poll_init;
#else
volatile bool _bal_async_poll_init = false;
#endif

/* async I/O state container. */
bal_as_container _bal_as_container = {
    NULL,
    BAL_MUTEX_INIT,
    BAL_THREAD_INIT,
    0
};

/* global library state. */
bal_state _bal_state = {
    BAL_MUTEX_INIT,
    0u
};
