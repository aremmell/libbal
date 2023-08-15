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

# include "balplatform.h"

/** The sockaddr_storage wrapper type. */
typedef struct sockaddr_storage bal_sockaddr;

struct bal_socket; /* forward declaration. */

/** bal_asyncselect callback. */
typedef void (*bal_async_cb)(struct bal_socket*, uint32_t);

/** List iteration callback. Returns false to stop iteration. */
typedef bool (*bal_list_iter_cb)(bal_descriptor /*key*/,
    struct bal_socket* /*val*/, void* /*ctx*/);

/** Worker thread callback. */
typedef bal_threadret (*bal_thread_cb)(void*);

typedef struct bal_state {
    uint32_t mask;     /**< State bitmask. */
    bal_async_cb proc; /**< Async I/O event callback. */
} bal_state;

typedef struct bal_socket {
    bal_descriptor sd; /**< Socket descriptor. */
    int addr_fam;      /**< Address family (e.g. AF_INET). */
    int type;          /**< Socket type (e.g., SOCK_STREAM). */
    int proto;         /**< Protocol (e.g., IPPROTO_TCP). */
    bal_state state;   /**< Internal socket state data. */
    // bal_mutex m;    /**< Mutex guard for socket state data. */
} bal_socket;

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
    char addr[NI_MAXHOST];
    const char* type;
    char port[NI_MAXSERV];
} bal_addrstrings;

typedef struct {
    int code;
    char desc[BAL_MAXERROR];
} bal_error;

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

typedef struct {
    bal_descriptor key;
    bal_socket** val;
    bool found;
} _bal_list_find_data;

typedef struct {
    bal_list* lst;        /** List of active socket descriptors and their states. */
    bal_mutex mutex;      /** Mutex for access to `lst`. */
    bal_thread thread;    /** Asynchronous I/O events thread. */

    bal_list* lst_add;    /** List of socket descriptors to add to `lst`. */
    bal_list* lst_rem;    /** List of socket descriptors to remove from `lst`. */
    bal_condition s_cond; /** Condition variable for `lst_add` and `lst_rem`. */
    bal_mutex s_mutex;    /** Mutex for access to `lst_add` and `lst_rem`. */
    bal_thread s_thread;  /** Data synchronization thread. */

# if defined(__HAVE_STDATOMICS__) && !defined(__cplusplus)
    atomic_bool die;
# else
    volatile bool die;
# endif
} bal_as_container;

#endif /* !_BAL_TYPES_H_INCLUDED */
