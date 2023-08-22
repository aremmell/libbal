/*
 * types.h
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

# include "platform.h"

/** The sockaddr_storage wrapper type. */
typedef struct sockaddr_storage bal_sockaddr;

struct bal_socket; /* forward declaration. */

/** bal_async_poll callback. */
typedef void (*bal_async_cb)(struct bal_socket*, uint32_t);

/** List iteration callback. Returns false to stop iteration. */
typedef bool (*bal_list_iter_cb)(bal_descriptor /*key*/,
    struct bal_socket* /*val*/, void* /*ctx*/);

/** Worker thread callback. */
typedef bal_threadret (*bal_thread_cb)(void*);


typedef struct bal_socket {
    bal_descriptor sd;     /**< Socket descriptor. */
    int addr_fam;          /**< Address family (e.g. AF_INET). */
    int type;              /**< Socket type (e.g., SOCK_STREAM). */
    int proto;             /**< Protocol (e.g., IPPROTO_TCP). */
    struct {               /**< Internal socket state data. */
        uint32_t mask;     /**< Async I/O event mask. */
        uint32_t bits;     /**< State bitmask. */
        bal_async_cb proc; /**< Async I/O event callback. */
    } state;
} bal_socket;

typedef struct _bal_addr {
    bal_sockaddr addr;
    struct _bal_addr* next;
} bal_addr;

typedef struct {
    bal_addr* addr;
    bal_addr* iter;
} bal_addrlist;

typedef struct {
    char host[NI_MAXHOST];
    char addr[NI_MAXHOST];
    const char* type;
    char port[NI_MAXSERV];
} bal_addrstrings;

/** The public error type. */
typedef struct {
    int code;
    char message[BAL_MAXERROR];
} bal_error;

/** The internal error type. */
typedef struct {
    int code;
    struct {
        const char* func;
        const char* file;
        uint32_t line;
    } loc;
    struct {
        int code;
        char message[BAL_MAXERROR];
    } os;
} bal_thread_error_info;

/* Node type for bal_list. */
typedef struct _bal_list_node {
    bal_descriptor key;
    bal_socket* val;
    struct _bal_list_node *prev;
    struct _bal_list_node *next;
} bal_list_node;

/* List of socket descriptors and associated state data. */
typedef struct {
    bal_list_node* head;
    bal_list_node* iter;
} bal_list;

/* Iteration callback. Returns false to stop iteration. */
typedef bool (*bal_list_iter_callback)(bal_descriptor /*key*/,
    bal_socket* /*val*/, void* /*ctx*/);

/* List of socket descriptors and associated state data. */
typedef struct {
    bal_descriptor key;
    bal_socket** val;
    bool found;
} bal_list_find_data;

typedef struct {
    bal_list* lst;        /** List of active socket descriptors and their states. */
    bal_mutex mutex;      /** Mutex for access to `lst`. */
    bal_thread thread;    /** Asynchronous I/O events thread. */
# if defined(__HAVE_STDATOMICS__) && !defined(__cplusplus)
    atomic_bool die;
# else
    volatile bool die;
# endif
} bal_as_container;

typedef struct {
    bal_mutex mutex;
# if defined(__HAVE_STDATOMICS__) && !defined(__cplusplus)
    atomic_uint_fast32_t magic;
# else
    volatile uint_fast32_t magic;
# endif
} bal_state;

#endif /* !_BAL_TYPES_H_INCLUDED */
