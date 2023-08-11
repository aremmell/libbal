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

/*─────────────────────────────────────────────────────────────────────────────╮
│                             Internal functions                               │
╰─────────────────────────────────────────────────────────────────────────────*/


int _bal_bindany(const bal_socket* s, unsigned short port);
int _bal_getaddrinfo(int f, int af, int st, const char* host, const char* port,
    bal_addrinfo* res);
int _bal_getnameinfo(int f, const bal_sockaddr* in, char* host, char* port);

const struct addrinfo* _bal_enumaddrinfo(bal_addrinfo* ai);
int _bal_aitoal(bal_addrinfo* in, bal_addrlist* out);

int _bal_getlasterror(const bal_socket* s, bal_error* err);
void __bal_setlasterror(int err, const char* func, const char* file, int line);
# define _bal_setlasterror(err) __bal_setlasterror(err, __func__, __file__, __LINE__);

int _bal_retstr(char* out, const char* in, size_t destlen);

int _bal_haspendingconnect(const bal_socket* s);
int _bal_isclosedcircuit(const bal_socket* s);

BALTHREAD _bal_eventthread(void* p);
int _bal_initasyncselect(bal_thread* t, bal_mutex* m, bal_eventthread_data* td);
int _bal_cleanupasyncselect(bal_thread* t, bal_mutex* m, bal_eventthread_data* td);

void _bal_dispatchevents(fd_set* set, bal_eventthread_data * td, uint32_t type);

/** Creates a new list. */
bool _bal_list_create(bal_list** lst);

/** Creates a node and assigns key and value to it. */
bool _bal_list_create_node(bal_list_node** node, bal_descriptor key,
    bal_selectdata* val);

/** Appends a node with the supplied key and value to the end of the list. */
bool _bal_list_add(bal_list* lst, bal_descriptor key, bal_selectdata* val);

/** Finds a node by key and sets `val` to its value, if found. */
bool _bal_list_find(bal_list* lst, bal_descriptor key, bal_selectdata** val);

/** True if the list contains zero nodes. */
bool _bal_list_empty(bal_list* lst);

/** Calls `func` for each node in the list, passing `ctx`. */
bool _bal_list_iterate(bal_list* lst, void* ctx, bal_list_iter_callback cb);

/** Finds a node by key, and destroys it if found. */
bool _bal_list_remove(bal_list* lst, bal_descriptor key, bal_selectdata** val);

/** Removes and deallocates all nodes from the list, leaving the list empty. */
bool _bal_list_remove_all(bal_list* lst);

/** Deallocates any nodes in the list and deallocates the list. */
bool _bal_list_destroy(bal_list** lst);

/** Deallocates a node and set prev/next pointers on its neighbors accordingly. */
bool _bal_list_destroy_node(bal_list_node** node);

/** Callback for finding nodes by key. */
bool __bal_list_find_key(bal_descriptor key, bal_selectdata* val, void* ctx);

/** Callback for dispatching async events. */
bool __bal_list_dispatch_events(bal_descriptor key, bal_selectdata* val, void* ctx);

/** Callback for removing entries from a list. */
bool __bal_list_remove_entries(bal_descriptor key, bal_selectdata* val, void* ctx);

/** Callback for the event handler thread. */
bool __bal_list_event_prepare(bal_descriptor key, bal_selectdata* val, void* ctx);

int _bal_mutex_init(bal_mutex* m);
int _bal_mutex_lock(bal_mutex* m);
int _bal_mutex_unlock(bal_mutex* m);
int _bal_mutex_destroy(bal_mutex* m);

/** Creates/initializes a new condition variable. */
bool _bal_condcreate(bal_condition* cond);

/** Signals a condition variable. */
bool _bal_condsignal(bal_condition* cond);

/** Broadcast signals a condition variable. */
bool _bal_condbroadcast(bal_condition* cond);

/** Destroys a condition variable. */
bool _bal_conddestroy(bal_condition* cond);

/** Waits indefinitely for a condition variable to become signaled. */
bool _bal_condwait(bal_condition* cond, bal_mutex* mutex);

/** Waits a given amount of time for a condition variable to become signaled. */
bool _bal_condwait_timeout(bal_condition* cond, bal_mutex* mutex, bal_wait* how_long);

#if defined(__HAVE_STDATOMICS__)
bool _bal_get_boolean(atomic_bool* boolean);
void _bal_set_boolean(atomic_bool* boolean, bool value);
#else
bool _bal_get_boolean(bool* boolean);
void _bal_set_boolean(bool* boolean, bool value);
#endif

void _bal_yield_thread(void);

#if defined(BAL_SELFLOG)
void __bal_selflog(const char* func, const char* file, uint32_t line,
    const char* format, ...);
# define _bal_selflog(...) \
    __bal_selflog(__func__, __file__, __LINE__, __VA_ARGS__);
#else
static inline
void __dummy_func(const char* dummy, ...) { BAL_UNUSED(dummy); }
# define _bal_selflog(...) __dummy_func(__VA_ARGS__)
#endif

static inline
void _bal_safefree(void** pp)
{
    if (pp && *pp) {
        free(*pp);
        *pp = NULL;
    }
}

# define bal_safefree(pp) _bal_safefree((void**)pp)

static inline
bool _bal_validptr(void* p) {
    return NULL != p;
}

bool _bal_once(bal_once* once, bal_once_fn func);

# if defined(__WIN__)
BOOL CALLBACK _bal_static_once_init(PINIT_ONCE ponce, PVOID param, PVOID* ctx);
# else
void _bal_static_once_init(void);
# endif

#endif /* !_BAL_INTERNAL_H_INCLUDED */
