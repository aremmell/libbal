/*
 * balinternal.h
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
#ifndef _BAL_INTERNAL_H_INCLUDED
# define _BAL_INTERNAL_H_INCLUDED

# include "balplatform.h"
# include "baltypes.h"

/******************************************************************************\
 *                             Internal Functions                             *
\******************************************************************************/

#if defined(__cplusplus)
extern "C" {
#endif

bool _bal_init(void);
bool _bal_cleanup(void);

int _bal_asyncselect(bal_socket* s, bal_async_cb proc, uint32_t mask);

bool _bal_initasyncselect(void);
bool _bal_cleanupasyncselect(void);

int _bal_sock_destroy(bal_socket** s);

bool _bal_defer_add_socket(bal_socket* s);
bool _bal_defer_remove_socket(bal_descriptor sd, bal_socket* s);

int _bal_bindany(const bal_socket* s, unsigned short port);
int _bal_getaddrinfo(int f, int af, int st, const char* host, const char* port,
    struct addrinfo** res);
int _bal_getnameinfo(int f, const bal_sockaddr* in, char* host, char* port);

int _bal_aitoal(struct addrinfo* ai, bal_addrlist* out);

int _bal_retstr(char* out, const char* in, size_t destlen);

bool _bal_islistening(bal_socket* s);
bool _bal_haspendingconnect(bal_socket* s);
bool _bal_isclosedcircuit(const bal_socket* s);

bal_threadret _bal_eventthread(void* ctx);
bal_threadret _bal_syncthread(void* ctx);

typedef bal_threadret (*bal_thread_func)(void*);

void _bal_processevents(bal_list* lst, uint32_t type);
void _bal_dispatchevents(const fd_set* set, bal_descriptor sd, bal_socket* s,
    uint32_t type);

/** Creates a new list. */
bool _bal_list_create(bal_list** lst);

/** Creates a node and assigns key and value to it. */
bool _bal_list_create_node(bal_list_node** node, bal_descriptor key,
    bal_socket* val);

/** Appends a node with the supplied key and value to the end of the list. */
bool _bal_list_add(bal_list* lst, bal_descriptor key, bal_socket* val);

/** Finds a node by key and sets `val` to its value, if found. */
bool _bal_list_find(bal_list* lst, bal_descriptor key, bal_socket** val);

/** True if the list contains zero nodes. */
bool _bal_list_empty(const bal_list* lst);

/** Retrieves the key and value for the current iterator, if set. If further
 * nodes exist, advances the iterator. */
bool _bal_list_iterate(bal_list* lst, bal_descriptor* key, bal_socket** val);

/** Resets the iterator to the first node in the list. */
void _bal_list_reset_iterator(bal_list* lst);

/** Calls `func` for each node in the list, passing `ctx`. */
bool _bal_list_iterate_func(bal_list* lst, void* ctx, bal_list_iter_callback cb);

/** Finds a node by key, and destroys it if found. */
bool _bal_list_remove(bal_list* lst, bal_descriptor key, bal_socket** val);

/** Removes and deallocates all nodes from the list, leaving the list empty. */
bool _bal_list_remove_all(bal_list* lst);

/** Deallocates any nodes in the list and deallocates the list. */
bool _bal_list_destroy(bal_list** lst);

/** Deallocates a node and set prev/next pointers on its neighbors accordingly. */
bool _bal_list_destroy_node(bal_list_node** node);

/** Callback for finding nodes by key. */
bool __bal_list_find_key(bal_descriptor key, bal_socket* val, void* ctx);

/** Callback for removing entries from a list. */
bool __bal_list_remove_entries(bal_descriptor key, bal_socket* val, void* ctx);

/** Callback for adding entries to a list. */
bool __bal_list_add_entries(bal_descriptor key, bal_socket* val, void* ctx);

/** Creates/initializes a new mutex. */
bool _bal_mutex_create(bal_mutex* mutex);

/** Attempts to lock a mutex and waits indefinitely. */
bool _bal_mutex_lock(bal_mutex* mutex);

/** Determines if a mutex is locked without waiting. */
bool _bal_mutex_trylock(bal_mutex* mutex);

/** Unlocks a previously locked mutex. */
bool _bal_mutex_unlock(bal_mutex* mutex);

/** Destroys a mutex. */
bool _bal_mutex_destroy(bal_mutex* mutex);

/** Creates/initializes a new condition variable. */
bool _bal_cond_create(bal_condition* cond);

/** Signals a condition variable. */
bool _bal_cond_signal(bal_condition* cond);

/** Broadcast signals a condition variable. */
bool _bal_cond_broadcast(bal_condition* cond);

/** Destroys a condition variable. */
bool _bal_cond_destroy(bal_condition* cond);

/** Waits indefinitely for a condition variable to become signaled. */
bool _bal_cond_wait(bal_condition* cond, bal_mutex* mutex);

/** Waits a given amount of time for a condition variable to become signaled. */
bool _bal_condwait_timeout(bal_condition* cond, bal_mutex* mutex, bal_wait* how_long);

# if defined(__HAVE_STDATOMICS__)
bool _bal_get_boolean(const atomic_bool* boolean);
void _bal_set_boolean(atomic_bool* boolean, bool value);
# else
bool _bal_get_boolean(const bool* boolean);
void _bal_set_boolean(bool* boolean, bool value);
# endif

/* void _bal_init_socket(bal_socket* s); */

bool _bal_once(bal_once* once, bal_once_fn func);

pid_t _bal_gettid(void);

# if defined(__WIN__)
BOOL CALLBACK _bal_static_once_init_func(PINIT_ONCE ponce, PVOID param, PVOID* ctx);
# else
void _bal_static_once_init_func(void);
# endif

# define _BAL_SASIZE(sa) \
    ((PF_INET6 == ((struct sockaddr* )&(sa))->sa_family) \
        ? sizeof(struct sockaddr_in6) : sizeof(struct sockaddr_in))

# define _BAL_NI_NODNS (NI_NUMERICHOST | NI_NUMERICSERV)
# define _BAL_NI_DNS   (NI_NAMEREQD | NI_NUMERICSERV)

static inline
void __bal_safefree(void** pp)
{
    if (pp && *pp) {
        free(*pp);
        *pp = NULL;
    }
}

# define _bal_safefree(pp) __bal_safefree((void**)(pp))

# define _bal_validptr(p) (NULL != (p))

# define _bal_validptrptr(pp) (NULL != (pp))

# define bal_countof(arr) (sizeof((arr)) / sizeof((arr)[0]))

# define _bal_validstr(str) ((str) && (*str))

# define _BAL_ENTER_MUTEX(m, name) \
    bool name##_locked = _bal_mutex_lock(m); \
    BAL_ASSERT(name##_locked); \
    if (name##_locked) {

#define _BAL_LEAVE_MUTEX(m, name) \
    bool name##_unlocked = _bal_mutex_unlock(m); \
    BAL_ASSERT_UNUSED(name##_unlocked, name##_unlocked); \
    }

/** Converts a bal_socket into human-readable form. */
void _bal_socket_tostr(const bal_socket* s, char buf[256]);

#if defined(__cplusplus)
}
#endif

#endif /* !_BAL_INTERNAL_H_INCLUDED */
