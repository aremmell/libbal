/*
 * baltypes.h
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
#ifndef _BAL_TYPES_H_INCLUDED
# define _BAL_TYPES_H_INCLUDED

#include "balplatform.h"

/**
 * @struct bal_socket
 * @brief BAL state structure.
 */
typedef struct {
    bal_descriptor sd; /**< Socket descriptor. */
    int af;            /**< Address family (e.g. AF_INET). */
    int st;            /**< Socket type (e.g., SOCK_STREAM). */
    int pf;            /**< Protocol family (e.g., IPPROTO_TCP). */
    uint32_t _f;       /**< Internally-used state flags. */
} bal_socket;

typedef struct {
    struct addrinfo* _ai;
    struct addrinfo* _p;
} bal_addrinfo;

typedef struct _bal_addr {
    bal_sockaddr _sa;
    struct _bal_addr* _n;
} bal_addr;

typedef struct {
    bal_addr* _a;
    bal_addr* _p;
} bal_addrlist;

typedef struct {
    char host[NI_MAXHOST];
    char ip[NI_MAXHOST];
    char* type;
    char port[NI_MAXSERV];
} bal_addrstrings;

typedef struct {
    int code;
    char desc[BAL_MAXERROR];
} bal_error;

typedef void (*bal_async_callback)(const bal_socket*, int);

typedef struct bal_selectdata {
    bal_socket* s;
    uint32_t mask;
    bal_async_callback proc;
    struct bal_selectdata* _p;
    struct bal_selectdata* _n;
} bal_selectdata;

typedef struct {
    bal_selectdata* _h;
    bal_selectdata* _t;
    bal_selectdata* _c;
} bal_selectdata_list;

typedef struct {
    bal_selectdata_list* sdl;
    bal_mutex* m;
#if !defined(__STDC_NO_ATOMICS__) && !defined(__cplusplus)
    atomic_bool die;
#else
    volatile bool die;
#endif
} bal_eventthread_data;

#endif /* !_BAL_TYPES_H_INCLUDED */
