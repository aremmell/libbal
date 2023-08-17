/*
 * balinternal.c
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
#include "bal/internal.h"
#include "bal/helpers.h"
#include "bal.h"

/******************************************************************************\
 *                               Static globals                               *
\******************************************************************************/

/* one-time initializer for async select container. */
static bal_once _bal_static_once_init = BAL_ONCE_INIT;

/* whether or not the async select handler is initialized. */
#if defined(__HAVE_STDATOMICS__)
static atomic_bool _bal_asyncselect_init;
#else
static volatile bool _bal_asyncselect_init = false;
#endif

/* async I/O state container. */
static bal_as_container _bal_as_container = {0};

/******************************************************************************\
 *                             Internal Functions                             *
\******************************************************************************/

bool _bal_init(void)
{
#if defined(__WIN__)
    WORD wVer = MAKEWORD(2, 2);
    WSADATA wd = {0};

    if (0 != WSAStartup(wVer, &wd)) {
        _bal_handlelasterr();
        return false;
    }
#endif

    bool init = _bal_once(&_bal_static_once_init, &_bal_static_once_init_func);
    BAL_ASSERT(init);

    if (!init) {
        _bal_dbglog("error: _bal_static_once_init_func failed");
        return false;
    }

    init = _bal_initasyncselect();
    BAL_ASSERT(init);

    if (!init) {
        _bal_dbglog("error: _bal_initasyncselect failed");
        bool cleanup = _bal_cleanupasyncselect();
        BAL_ASSERT_UNUSED(cleanup, cleanup);
        return false;
    }

    return true;
}

bool _bal_cleanup(void)
{
#if defined(__WIN__)
    (void)WSACleanup();
#endif

    if (!_bal_cleanupasyncselect()) {
        _bal_dbglog("error: _bal_cleanupasyncselect failed");
        return false;
    }

    return true;
}

int _bal_asyncselect(bal_socket* s, bal_async_cb proc, uint32_t mask)
{
    if (!_bal_get_boolean(&_bal_asyncselect_init)) {
        _bal_dbglog("error: async I/O not initialized; ignoring");
        return BAL_FALSE;
    }

    if (_bal_get_boolean(&_bal_as_container.die)) {
        _bal_dbglog("error: async I/O shutting down; ignoring");
        return BAL_FALSE;
    }

    if (!_bal_validptr(s)) {
        BAL_ASSERT(NULL != s);
        return BAL_FALSE;
    }

    if (0 >= s->sd) {
        BAL_ASSERT(BAL_BADSOCKET != s->sd && 0 != s->sd);
        return BAL_FALSE;
    }

    if (!_bal_validptr(proc) && 0U != mask) {
        BAL_ASSERT(NULL != proc || 0U == mask);
        return BAL_FALSE;
    }

    int r = BAL_FALSE;
    if (_bal_mutex_lock(&_bal_as_container.mutex)) {
        if (0U == mask) {
            /* this thread holds the mutex that guards the list, so in theory,
             * a remove operation should be possible (even if it's the current
             * iterator) without an issue, as long as no further access to the
             * iterator is attempted. */
            bal_socket* d = NULL;
            bool success  = _bal_list_remove(_bal_as_container.lst, s->sd, &d);

            BAL_ASSERT(NULL != d && s == d);
            if (success) {
                /* The iterator is kaput, but d and s are still allocated.
                 * Since this is just a removal request, (mask = 0), don't close
                 * or delete the socket itself–just the data. The caller will
                 * have to call bal_sock_destroy later on. */
                _bal_dbglog("removed socket "BAL_SOCKET_SPEC" (%p) from list",
                            s->sd, d);
                r = BAL_TRUE;
            } else {
                _bal_dbglog("warning: socket "BAL_SOCKET_SPEC" not found;"
                            " ignoring removal request", s->sd);
            }
        } else {
            bal_socket* d = NULL;
            if (_bal_list_find(_bal_as_container.lst, s->sd, &d)) {
                BAL_ASSERT(NULL != d && s == d);
                s->state.mask = mask;
                s->state.proc = proc;
                r             = BAL_TRUE;
                _bal_dbglog("updated socket "BAL_SOCKET_SPEC" (%p)", s->sd, s);
            } else {
                bool success = false;
                if (BAL_FALSE == bal_setiomode(s, true)) {
                    _bal_handlelasterr();
                } else {
                    s->state.mask = mask;
                    s->state.proc = proc;
                    success = _bal_list_add(_bal_as_container.lst, s->sd, s);
                    r       = success ? BAL_TRUE : BAL_FALSE;
                }
                if (success) {
                    _bal_dbglog("added socket "BAL_SOCKET_SPEC" to list (%p"
                                ", mask = %08x)", s->sd, s, s->state.mask);
                } else {
                    _bal_dbglog("error: failed to add socket "BAL_SOCKET_SPEC
                                " to list!", s->sd);
                }
            }
        }

        bool unlocked = _bal_mutex_unlock(&_bal_as_container.mutex);
        BAL_ASSERT_UNUSED(unlocked, unlocked);
    }

    return r;
}

bool _bal_initasyncselect(void)
{
    bal_list** lists[] = {
        &_bal_as_container.lst/* ,
        &_bal_as_container.lst_add,
        &_bal_as_container.lst_rem */
    };

    bool init = true;
    for (size_t n = 0; n < _bal_countof(lists); n++) {
        init &= _bal_list_create(lists[n]);
        BAL_ASSERT(init);

        if (!init) {
            _bal_handlelasterr();
            _bal_dbglog("error: failed to create list(s)");
            return false;
        }
    }

    bal_mutex* mutexes[] = {
        &_bal_as_container.mutex/* ,
        &_bal_as_container.s_mutex */
    };

    for (size_t n = 0; n < _bal_countof(mutexes); n++) {
        init &= _bal_mutex_create(mutexes[n]);
        BAL_ASSERT(init);

        if (!init) {
            _bal_dbglog("error: failed to create mutex(es)");
            return false;
        }
    }

    /*bal_condition* conditions[] = {
         &_bal_as_container.s_cond
    };

    for (size_t n = 0; n < _bal_countof(conditions); n++) {
        init &= _bal_cond_create(conditions[n]);
        BAL_ASSERT(init);

        if (!init) {
            _bal_dbglog("error: failed to create condition variable(s)");
            return false;
        }
    }*/

    struct {
        bal_thread* thread;
        bal_thread_cb proc;
    } threads[] = {
        {
            &_bal_as_container.thread,
            &_bal_eventthread
        }/* ,
        {
            &_bal_as_container.s_thread,
            &_bal_syncthread
        } */
    };

    for (size_t n = 0; n < _bal_countof(threads); n++) {
#if defined(__WIN__)
        *threads[n].thread = _beginthreadex(NULL, 0U, threads[n].proc,
            &_bal_as_container, 0U, NULL);
        BAL_ASSERT(0ULL != *threads[n].thread);

        create &= 0ULL != *threads[n].thread;

        if (0ULL == *threads[n].thread) {
            _bal_handlelasterr();
            return false;
        }
#else
        int op = pthread_create(threads[n].thread, NULL, threads[n].proc,
            &_bal_as_container);
        BAL_ASSERT(0 == op);

        init &= 0 == op;

        if (0 != op)
            return _bal_handleerr(op);
#endif
    }

    _bal_set_boolean(&_bal_asyncselect_init, true);
    _bal_dbglog("async I/O initialized %s", init ? "successfully" : "with errors");

    return init;
}

bool _bal_cleanupasyncselect(void)
{
    _bal_set_boolean(&_bal_as_container.die, true);

    bal_thread* threads[] = {
        &_bal_as_container.thread/* ,
        &_bal_as_container.s_thread */
    };

    _bal_dbglog("joining %zu thread(s)...", _bal_countof(threads));
    for (size_t n = 0; n < _bal_countof(threads); n++) {
#if defined(__WIN__)
        DWORD wait = WaitForSingleObject((HANDLE)*threads[n], INFINITE);
        BAL_ASSERT_UNUSED(wait, WAIT_OBJECT_0 == wait);
#else
        int wait = pthread_join(*threads[n], NULL);
        BAL_ASSERT_UNUSED(wait, 0 == wait);
        if (0 != wait)
            (void)_bal_handleerr(wait);
#endif
    }

    bool cleanup = true;
    bal_list* lists[] = {
        _bal_as_container.lst/* ,
        _bal_as_container.lst_add,
        _bal_as_container.lst_rem */
    };

    for (size_t n = 0UL; n < _bal_countof(lists); n++) {
         if (0 == n && !_bal_list_empty(lists[n])) {
            _bal_dbglog("warning: list is not empty:");

            bal_descriptor key = 0;
            bal_socket* val    = NULL;

            _bal_list_reset_iterator(lists[n]);
            while (_bal_list_iterate(lists[n], &key, &val)) {
                _bal_dbglog("warning: dangling bal_socket "BAL_SOCKET_SPEC" (%p)",
                    key, val);
            }
         }

        bool destroy = _bal_list_destroy(&lists[n]);
        BAL_ASSERT(destroy);
        cleanup &= destroy;
    }

    bal_mutex* mutexes[] = {
        &_bal_as_container.mutex/* ,
        &_bal_as_container.s_mutex */
    };

    for (size_t n = 0; n < _bal_countof(mutexes); n++) {
        bool destroy = _bal_mutex_destroy(mutexes[n]);
        BAL_ASSERT(destroy);
        cleanup &= destroy;
    }

    /* bal_condition* conditions[] = {
        &_bal_as_container.s_cond
    };

    for (size_t n = 0; n < _bal_countof(conditions); n++) {
        bool destroy = _bal_cond_destroy(conditions[n]);
        BAL_ASSERT(destroy);
        cleanup &= destroy;
    }*/

    _bal_set_boolean(&_bal_asyncselect_init, false);
    _bal_dbglog("async I/O cleaned up %s", cleanup ? "successfully" : "with errors");

    return cleanup;
}

int _bal_sock_destroy(bal_socket** s)
{
    int r = BAL_FALSE;

    if (_bal_validptrptr(s) && _bal_validptr(*s)) {
        _BAL_ENTER_MUTEX(&_bal_as_container.mutex, sock_destroy);

        /* just to be safe, ensure that the socket is not currently in the
         * async I/O list. */
        bal_socket* d = NULL;
        bool removed  = _bal_list_remove(_bal_as_container.lst, (*s)->sd, &d);

        if (removed) {
            BAL_ASSERT(*s == d);
            _bal_dbglog("removed socket "BAL_SOCKET_SPEC" (%p) from list",
                (*s)->sd, *s);
        }

        if (!bal_isbitset((*s)->state.bits, BAL_S_CLOSE)) {
            _bal_dbglog("warning: freeing possibly open socket "BAL_SOCKET_SPEC
                        " (%p)", (*s)->sd, *s);
        } else {
            _bal_dbglog("freeing socket "BAL_SOCKET_SPEC" (%p)", (*s)->sd, *s);
        }

        memset(*s, 0, sizeof(bal_socket));
        _bal_safefree(s);
        _BAL_LEAVE_MUTEX(&_bal_as_container.mutex, sock_destroy);
        r = BAL_TRUE;
    }

    return r;
}

/*bool _bal_defer_add_socket(bal_socket* s)
{
    bool retval = false;

    if (_bal_validsock(s)) {
        bool locked = _bal_mutex_lock(&_bal_as_container.s_mutex);
        BAL_ASSERT(locked);

        if (locked) {
            retval = _bal_list_add(_bal_as_container.lst_add, s->sd, s) &&
                _bal_cond_broadcast(&_bal_as_container.s_cond);
            bool unlocked = _bal_mutex_unlock(&_bal_as_container.s_mutex);
            BAL_ASSERT_UNUSED(unlocked, unlocked);
        }
    }

    return retval;
}*/

/*bool _bal_defer_remove_socket(bal_descriptor sd, bal_socket* s)
{
    bool retval = false;

    if (_bal_validsock(s)) {
        bool locked = _bal_mutex_lock(&_bal_as_container.s_mutex);
        BAL_ASSERT(locked);

        if (locked) {
            retval = _bal_list_add(_bal_as_container.lst_rem, sd, s) &&
                _bal_cond_broadcast(&_bal_as_container.s_cond);
            bool unlocked = _bal_mutex_unlock(&_bal_as_container.s_mutex);
            BAL_ASSERT_UNUSED(unlocked, unlocked);
        }
    }

    return retval;
}*/

int _bal_getaddrinfo(int f, int af, int st, const char* host, const char* port,
    struct addrinfo** res)
{
    int r = BAL_FALSE;

    if (_bal_validstr(host) && _bal_validptrptr(res)) {
        struct addrinfo hints = {0};

        hints.ai_flags    = f;
        hints.ai_family   = af;
        hints.ai_socktype = st;

        r = getaddrinfo(host, port, (const struct addrinfo*)&hints, res);
    }

    if (0 != r) {
        _bal_handlegaierr(r);
        r = BAL_FALSE;
    }

    return r;
}

int _bal_getnameinfo(int f, const bal_sockaddr* in, char* host, char* port)
{
    int r = BAL_FALSE;

    if (_bal_validptr(in) && _bal_validptr(host)) {
        socklen_t inlen = _BAL_SASIZE(*in);
        r = getnameinfo((const struct sockaddr*)in, inlen, host, NI_MAXHOST, port,
            NI_MAXSERV, f);
    }

    if (0 != r) {
        _bal_handlegaierr(r);
        r = BAL_FALSE;
    }

    return r;
}

bool _bal_haspendingconnect(bal_socket* s)
{
    return _bal_validsock(s) && bal_isbitset(s->state.bits, BAL_S_CONNECT);
}

uint32_t _bal_on_pending_conn_event(bal_socket* s)
{
    uint32_t r  = 0U;
    bool closed = _bal_isclosedcircuit(s);

    if (0 != bal_geterror(s) || closed) {
        r = BAL_E_CONNFAIL;
    } else {
        r = BAL_E_CONNECT;
    }

    s->state.mask &= ~BAL_E_WRITE;
    s->state.bits &= ~BAL_S_CONNECT;
    return r;
}

bool _bal_isclosedcircuit(const bal_socket* s)
{
    if (!_bal_validsock(s))
        return true;

    int buf = 0;
    int rcv = recv(s->sd, &buf, sizeof(int), MSG_PEEK | MSG_DONTWAIT);

    if (0 == rcv) {
        return true;
    } else if (-1 == rcv) {
#if defined(__WIN__)
        int error = WSAGetLastError();
        if (WSAENETDOWN == error     || WSAENOTCONN == error  ||
            WSAEOPNOTSUPP == error   || WSAESHUTDOWN == error ||
            WSAECONNABORTED == error || WSAECONNRESET == error)
            return true;
#else
        int error = errno;
        if (ENETDOWN == error     || ENOTCONN == error     ||
            ECONNREFUSED == error || ESHUTDOWN == error    ||
            ECONNABORTED == error || ECONNRESET == error   ||
            ENETUNREACH == error  || ENETRESET == error    ||
            EHOSTDOWN   == error  || EHOSTUNREACH == error)
            return true;
#endif
    }

    return false;
}

uint32_t _bal_pollflags_tomask(short flags)
{
    uint32_t retval = 0U;

    if (bal_isbitset(flags, POLLIN))
        bal_setbitshigh(&retval, BAL_E_READ);

    if (bal_isbitset(flags, POLLOUT))
        bal_setbitshigh(&retval, BAL_E_WRITE);

    if (bal_isbitset(flags, POLLPRI))
        bal_setbitshigh(&retval, BAL_E_EXCEPT);

    if (bal_isbitset(flags, POLLHUP))
        bal_setbitshigh(&retval, BAL_E_CLOSE);

    if (bal_isbitset(flags, POLLERR))
        bal_setbitshigh(&retval, BAL_E_ERROR);

    if (bal_isbitset(flags, POLLNVAL))
        bal_setbitshigh(&retval, BAL_E_INVALID);

    return retval;
}

short _bal_mask_topollflags(uint32_t mask)
{
    short retval = 0;

    if (bal_isbitset(mask, BAL_E_READ))
        bal_setbitshigh(&retval, POLLIN);

    if (bal_isbitset(mask, BAL_E_WRITE))
        bal_setbitshigh(&retval, POLLOUT);

    if (bal_isbitset(mask, BAL_E_EXCEPT))
        bal_setbitshigh(&retval, POLLPRI);

    if (bal_isbitset(mask, BAL_E_CLOSE))
        bal_setbitshigh(&retval, POLLHUP);

    if (bal_isbitset(mask, BAL_E_ERROR))
        bal_setbitshigh(&retval, POLLERR);

    if (bal_isbitset(mask, BAL_E_INVALID))
        bal_setbitshigh(&retval, POLLNVAL);

    return retval;
}

bal_threadret _bal_eventthread(void* ctx)
{
    static const int poll_timeout = 1000;
    bal_as_container* asc = (bal_as_container*)ctx;
    BAL_ASSERT(NULL != asc);

    while (!_bal_get_boolean(&asc->die)) {
        size_t count       = 0UL;
        struct pollfd* fds = NULL;
        _BAL_ENTER_MUTEX(&asc->mutex, dispatch)

        count = _bal_list_count(asc->lst);
        if (count > 0UL) {
            fds = calloc(count, sizeof(struct pollfd));
            BAL_ASSERT(NULL != fds);

            if (_bal_validptr(fds)) {
                size_t offset      = 0;
                bal_descriptor key = 0;
                bal_socket* val    = NULL;

                _bal_list_reset_iterator(asc->lst);
                while (_bal_list_iterate(asc->lst, &key, &val)) {
                    fds[offset].fd     = key;
                    fds[offset].events = _bal_mask_topollflags(val->state.mask);
                    offset++;
                }

                int res = poll(fds, count, poll_timeout);
                if (-1 == res) {
                    _bal_handlelasterr();
                } else if (res > 0) {
                    for (size_t n = 0UL; n < count; n++) {
                        bal_socket* s = NULL;
                        bool found    = _bal_list_find(asc->lst, fds[n].fd, &s);
                        BAL_ASSERT(found && _bal_validsock(s));

                        if (found && _bal_validsock(s)) {
                            uint32_t events = _bal_pollflags_tomask(fds[n].revents);
                            if (0U != events)
                                _bal_dispatchevents(fds[n].fd, s, events);
                        }
                    }
                }
                _bal_safefree(&fds);
            }
        }
        _BAL_LEAVE_MUTEX(&asc->mutex, dispatch)

        if (0 == count) {
            // TODO: figure out how to wait in an alertable state
            // when there's nothing to poll
            static const uint32_t empty_sleep = 100;
            bal_sleep_msec(empty_sleep);
        }

        bal_thread_yield();
    }

#if defined(__WIN__)
    return 0U;
#else
    return NULL;
#endif
}

/*bal_threadret _bal_syncthread(void* ctx)
{
    bal_as_container* asc = (bal_as_container*)ctx;
    BAL_ASSERT(NULL != asc);

    while (!_bal_get_boolean(&asc->die)) {
        if (_bal_mutex_lock(&asc->s_mutex)) {
            while (_bal_list_empty(asc->lst_add) && _bal_list_empty(asc->lst_rem) &&
                  !_bal_get_boolean(&asc->die)) {
#if !defined(__WIN__)
                / * seconds; absolute fixed time. * /
                bal_wait wait = {time(NULL) + 1, 0};
#else
                /    * msec; relative from now. * /
                bal_wait wait = 1000;
#endif
                (void)_bal_condwait_timeout(&asc->s_cond, &asc->s_mutex, &wait);
            }

            if (_bal_get_boolean(&asc->die)) {
                bool unlocked = _bal_mutex_unlock(&asc->s_mutex);
                BAL_ASSERT_UNUSED(unlocked, unlocked);
                break;
            }

            if (_bal_mutex_trylock(&asc->mutex)) {
                if (!_bal_list_empty(asc->lst_add)) {
                    _bal_dbglog("processing deferred socket adds...");

                    bool add = _bal_list_iterate_func(asc->lst_add, asc->lst,
                        &__bal_list_add_entries);
                    BAL_ASSERT_UNUSED(add, add);

                    add = _bal_list_remove_all(asc->lst_add);
                    BAL_ASSERT_UNUSED(add, add);
                }

                if (!_bal_list_empty(asc->lst_rem)) {
                    _bal_dbglog("processing deferred socket removals...");

                    bool rem = _bal_list_iterate_func(asc->lst_rem, asc->lst,
                        &__bal_list_remove_entries);
                    BAL_ASSERT_UNUSED(rem, rem);

                    rem = _bal_list_remove_all(asc->lst_rem);
                    BAL_ASSERT_UNUSED(rem, rem);
                }

                bool unlocked = _bal_mutex_unlock(&asc->mutex);
                BAL_ASSERT_UNUSED(unlocked, unlocked);
            }

            bool unlocked = _bal_mutex_unlock(&asc->s_mutex);
            BAL_ASSERT_UNUSED(unlocked, unlocked);
        }

        bal_thread_yield();
    }

#if defined(__WIN__)
    return 0U;
#else
    return NULL;
#endif
}*/

void _bal_dispatchevents(bal_descriptor sd, bal_socket* s, uint32_t events)
{
    uint32_t _events = 0U;

    if (bal_isbitset(s->state.mask, BAL_E_READ) &&
        bal_isbitset(events, BAL_E_READ)) {
        if (bal_islistening(s)) {
            _events |= BAL_E_ACCEPT;
        } else if (_bal_haspendingconnect(s)) {
            _events |= _bal_on_pending_conn_event(s);
        } else {
            _events |= BAL_E_READ;
        }
    }

    if (bal_isbitset(s->state.mask, BAL_E_WRITE) &&
        bal_isbitset(events, BAL_E_WRITE)) {
        if (_bal_haspendingconnect(s)) {
            _events |= _bal_on_pending_conn_event(s);
        } else if (bal_isbitset(s->state.mask, BAL_E_CLOSE) &&
            _bal_isclosedcircuit(s)) {
            _events |= BAL_E_CLOSE;
        } else {
            _events |= BAL_E_WRITE;
        }
    }

    if (bal_isbitset(s->state.mask, BAL_E_CLOSE) &&
        bal_isbitset(events, BAL_E_CLOSE)) {
        _events |= BAL_E_CLOSE;
    }

    if (bal_isbitset(s->state.mask, BAL_E_EXCEPT) &&
        bal_isbitset(events, BAL_E_EXCEPT)) {
        _events |= BAL_E_EXCEPT;
    }

    if (bal_isbitset(s->state.mask, BAL_E_ERROR) &&
        bal_isbitset(events, BAL_E_ERROR)) {
        _events |= BAL_E_ERROR;
    }

    bool close = bal_isbitset(events, BAL_E_CLOSE);

    if (0U != _events && _bal_validsock(s) && _bal_validptr(s->state.proc))
        s->state.proc(s, _events);

    if (close) {
        /* if the caller did the right thing, they have called bal_close,
         * and possibly even bal_sock_destroy. if they did not call the
         * latter, the socket still resides in the list. presume that
         * the caller knows what they're doing, and don't destroy the socket,
         * but remove it from the list. */
        bal_socket* d = NULL;
        bool removed  = _bal_list_remove(_bal_as_container.lst, sd, &d);

        if (removed) {
            _bal_dbglog("removed socket "BAL_SOCKET_SPEC" (%p) from list"
                        " (close event)", sd, s);
        } else {
            _bal_dbglog("socket "BAL_SOCKET_SPEC" destroyed by event"
                        " handler (close event)", sd);
        }
    }
}

bool _bal_list_create(bal_list** lst)
{
    bool retval = _bal_validptr(lst);

    if (retval) {
        *lst = calloc(1UL, sizeof(bal_list));
        retval = NULL != *lst;
    }

    return retval;
}

bool _bal_list_create_node(bal_list_node** node, bal_descriptor key,
    bal_socket* val)
{
    bool ok = _bal_validptrptr(node);

    if (ok) {
        *node = calloc(1UL, sizeof(bal_list_node));
        if (*node) {
            (*node)->key = key;
            (*node)->val = val;
        }
        ok = NULL != *node;
    }

    return ok;
}

bool _bal_list_add(bal_list* lst, bal_descriptor key, bal_socket* val)
{
    bool ok = _bal_validptr(lst);

    if (ok) {
        if (_bal_list_empty(lst)) {
            ok = _bal_list_create_node(&lst->head, key, val);
            lst->iter = lst->head;
        } else {
            bal_list_node* node = lst->head;
            while (node && node->next) node = node->next;

            ok = _bal_list_create_node(&node->next, key, val);
            if (ok)
                node->next->prev = node;
        }
    }

    return ok;
}

bool _bal_list_find(bal_list* lst, bal_descriptor key, bal_socket** val)
{
    bool ok = !_bal_list_empty(lst) && _bal_validptr(val);

    if (ok) {
        bal_list_find_data lfd = {key, val, false};
        ok = _bal_list_iterate_func(lst, &lfd, &__bal_list_find_key) && lfd.found;
    }

    return ok;
}

bool _bal_list_empty(const bal_list* lst)
{
    return !lst || !lst->head;
}

size_t _bal_list_count(bal_list* lst)
{
    size_t count = 0UL;

    if (!_bal_list_empty(lst)) {
        bal_descriptor key = 0;
        bal_socket* val    = NULL;

        _bal_list_reset_iterator(lst);
        while (_bal_list_iterate(lst, &key, &val)) {
            count++;
        }
    }

    return count;
}

bool _bal_list_iterate(bal_list* lst, bal_descriptor* key, bal_socket** val)
{
    bool ok = !_bal_list_empty(lst) && _bal_validptr(key) && _bal_validptr(val);

    if (ok) {
        ok = NULL != lst->iter;
        if (ok) {
            *key = lst->iter->key;
            *val = lst->iter->val;
            lst->iter = lst->iter->next;
        }
    }

    return ok;
}

void _bal_list_reset_iterator(bal_list* lst)
{
    if (_bal_validptr(lst))
        lst->iter = lst->head;
}

bool _bal_list_iterate_func(bal_list* lst, void* ctx, bal_list_iter_cb cb)
{
    bool ok = !_bal_list_empty(lst) && _bal_validptr(cb);

    if (ok) {
        size_t count        = 0UL;
        bal_list_node* node = lst->head;
        while (node) {
            count++;
            if (!cb(node->key, node->val, ctx))
                break;
            node = node->next;
        }
        ok = 0UL < count;
    }

    return ok;
}

bool _bal_list_remove(bal_list* lst, bal_descriptor key, bal_socket** val)
{
    bool ok = !_bal_list_empty(lst) && _bal_validptr(val);

    if (ok) {
        bool found          = false;
        bal_list_node* node = lst->head;
        while (node) {
            if (node->key == key) {
                found = true;
                if (node == lst->head)
                    lst->head = node->next;
                if (node == lst->iter)
                    lst->iter = lst->iter->prev;
                *val = node->val;
                ok   = _bal_list_destroy_node(&node);
                break;
            }
            node = node->next;
        }
        ok &= found;
    }

    return ok;
}

bool _bal_list_remove_all(bal_list* lst)
{
    bool ok = !_bal_list_empty(lst);

    if (ok) {
        bal_list_node* node = lst->head;
        while (node) {
            bal_list_node* next = node->next;
            ok  &= _bal_list_destroy_node(&node);
            node = next;
        }

        lst->head = NULL;
    }

    return ok;
}

bool _bal_list_destroy(bal_list** lst)
{
    bool ok = _bal_validptrptr(lst) && _bal_validptr(*lst);

    if (ok) {
        if (!_bal_list_empty(*lst))
            ok = _bal_list_remove_all(*lst);
        _bal_safefree(lst);
    }

    return ok;
}

bool _bal_list_destroy_node(bal_list_node** node)
{
    bool ok = _bal_validptrptr(node) && _bal_validptr(node);

    if (ok) {
        if ((*node)->prev)
            (*node)->prev->next = (*node)->next;

        if ((*node)->next)
            (*node)->next->prev = (*node)->prev;

        _bal_safefree(node);
    }

    return ok;
}

bool __bal_list_find_key(bal_descriptor key, bal_socket* val, void* ctx)
{
    bal_list_find_data* lfd = (bal_list_find_data*)ctx;
    if (key == lfd->key) {
        *lfd->val = val;
        lfd->found = true;
        return false;
    }
    return true;
}

/* bool __bal_list_remove_entries(bal_descriptor key, bal_socket* val, void* ctx)
{
    bal_list* lst = (bal_list*)ctx;
    bal_socket* d = NULL;
    bool removed  = _bal_list_remove(lst, key, &d);
    BAL_ASSERT_UNUSED(removed, removed);

    BAL_ASSERT(d == val);

    if (removed)
        _bal_dbglog("removed socket "BAL_SOCKET_SPEC" (%p) from list", key, val);

    return true;
} */

/* bool __bal_list_add_entries(bal_descriptor key, bal_socket* val, void* ctx)
{
    bal_list* lst = (bal_list*)ctx;
    bool added    = _bal_list_add(lst, key, val);
    BAL_ASSERT_UNUSED(added, added);

    if (added)
        _bal_dbglog("added socket "BAL_SOCKET_SPEC" (%p) to list", key, val);

    return true;
} */

#if !defined(__WIN__) /* pthread mutex implementation. */
bool _bal_mutex_create(bal_mutex* mutex)
{
    if (_bal_validptr(mutex)) {
        pthread_mutexattr_t attr;
        int op = pthread_mutexattr_init(&attr);
        if (0 == op) {
            op = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
            if (0 == op) {
                op = pthread_mutex_init(mutex, &attr);
                if (0 == op)
                    return true;
            }
        }
        (void)_bal_handleerr(op);
    }

    return false;
}

bool _bal_mutex_lock(bal_mutex* mutex)
{
    if (_bal_validptr(mutex)) {
        int op = pthread_mutex_lock(mutex);
        return 0 == op ? true : _bal_handleerr(op);
    }

    return false;
}

bool _bal_mutex_trylock(bal_mutex* mutex)
{
    if (_bal_validptr(mutex)) {
        int op = pthread_mutex_trylock(mutex);
        return 0 == op ? true : EBUSY != op ? _bal_handleerr(op) : false;
    }

    return false;
}

bool _bal_mutex_unlock(bal_mutex* mutex)
{
    if (_bal_validptr(mutex)) {
        int op = pthread_mutex_unlock(mutex);
        return 0 == op ? true : _bal_handleerr(op);
    }

    return false;
}

bool _bal_mutex_destroy(bal_mutex* mutex)
{
    if (_bal_validptr(mutex)) {
        int op = pthread_mutex_destroy(mutex);
        return 0 == op ? true : _bal_handleerr(op);
    }

    return false;
}
#else /* __WIN__ */
bool _bal_mutex_create(bal_mutex* mutex)
{
    if (_bal_validptr(mutex)) {
        InitializeCriticalSection(mutex);
        return true;
    }

    return false;
}

bool _bal_mutex_lock(bal_mutex* mutex)
{
    if (_bal_validptr(mutex)) {
        EnterCriticalSection(mutex);
        return true;
    }

    return false;
}

bool _bal_mutex_trylock(bal_mutex* mutex)
{
    if (_bal_validptr(mutex))
        return FALSE != TryEnterCriticalSection(mutex);

    return false;
}

bool _bal_mutex_unlock(bal_mutex* mutex)
{
    if (_bal_validptr(mutex)) {
        LeaveCriticalSection(mutex);
        return true;
    }

    return false;
}

bool _bal_mutex_destroy(bal_mutex* mutex)
{
    if (_bal_validptr(mutex)) {
        DeleteCriticalSection(mutex);
        return true;
    }

    return false;
}
#endif /* !__WIN__ */

#if !defined(__WIN__) /* pthread condition variable implementation. */
bool _bal_cond_create(bal_condition* cond)
{
    bool valid = _bal_validptr(cond);

    if (valid) {
        int op = pthread_cond_init(cond, NULL);
        valid = 0 == op ? true : _bal_handleerr(op);
    }

    return valid;
}

bool _bal_cond_signal(bal_condition* cond)
{
    bool valid = _bal_validptr(cond);

    if (valid) {
        int op = pthread_cond_signal(cond);
        valid = 0 == op ? true : _bal_handleerr(op);
    }

    return valid;
}

bool _bal_cond_broadcast(bal_condition* cond)
{
    bool valid = _bal_validptr(cond);

    if (valid) {
        int op = pthread_cond_broadcast(cond);
        valid = 0 == op ? true : _bal_handleerr(op);
    }

    return valid;
}

bool _bal_cond_destroy(bal_condition* cond)
{
    bool valid = _bal_validptr(cond);

    if (valid) {
        int op = pthread_cond_destroy(cond);
        valid = 0 == op ? true : _bal_handleerr(op);
    }

    return valid;
}

bool _bal_cond_wait(bal_condition* cond, bal_mutex* mutex)
{
    bool valid = _bal_validptr(cond) && _bal_validptr(mutex);

    if (valid) {
        int op = pthread_cond_wait(cond, mutex);
        valid = 0 == op ? true : _bal_handleerr(op);
    }

    return valid;
}

bool _bal_condwait_timeout(bal_condition* cond, bal_mutex* mutex, bal_wait* howlong)
{
    bool valid = _bal_validptr(cond) && _bal_validptr(mutex) && _bal_validptr(howlong);

    if (valid) {
        int op = pthread_cond_timedwait(cond, mutex, howlong);
        valid = 0 == op ? true : ETIMEDOUT == op ? false : _bal_handleerr(op);
    }

    return valid;
}
#else /* __WIN__ */
bool _bal_cond_create(bal_condition* cond)
{
    bool valid = _bal_validptr(cond);

    if (valid)
        InitializeConditionVariable(cond);

    return valid;
}

bool _bal_cond_signal(bal_condition* cond)
{
    bool valid = _bal_validptr(cond);

    if (valid)
        WakeConditionVariable(cond);

    return valid;
}

bool _bal_cond_broadcast(bal_condition* cond)
{
    bool valid = _bal_validptr(cond);

    if (valid)
        WakeAllConditionVariable(cond);

    return valid;
}

bool _bal_cond_destroy(bal_condition* cond)
{
    BAL_UNUSED(cond);
    return true;
}

bool _bal_cond_wait(bal_condition* cond, bal_mutex* mutex)
{
    DWORD how_long = INFINITE;
    return _bal_condwait_timeout(cond, mutex, &how_long);
}

bool _bal_condwait_timeout(bal_condition* cond, bal_mutex* mutex, bal_wait* how_long)
{
    bool valid = _bal_validptr(cond) && _bal_validptr(mutex) && _bal_validptr(how_long);

    if (valid)
        valid = (FALSE != SleepConditionVariableCS(cond, mutex, *how_long))
            ? true : (ERROR_TIMEOUT == GetLastError() ? false : _bal_handleerr(GetLastError()));

    return valid;
}
#endif

#if defined(__HAVE_STDATOMICS__)
bool _bal_get_boolean(const atomic_bool* boolean)
{
    bool retval = false;

    if (boolean)
        retval = atomic_load(boolean);

    return retval;
}

void _bal_set_boolean(atomic_bool* boolean, bool value)
{
    if (boolean)
        atomic_store(boolean, value);
}
#else
bool _bal_get_boolean(const bool* boolean)
{
    bool retval = false;

    if (boolean)
        retval = *boolean;

    return retval;
}

void _bal_set_boolean(bool* boolean, bool value)
{
    if (boolean)
        *boolean = value;
}
#endif

/* void _bal_init_socket(bal_socket* s)
{
    bool created = _bal_mutex_create(&s->m);
    BAL_ASSERT_UNUSED(created, created);
} */

bool _bal_once(bal_once* once, bal_once_fn func)
{
#if defined(__WIN__)
    return FALSE != InitOnceExecuteOnce(once, func, NULL, NULL);
#else
    return 0 == pthread_once(once, func);
#endif
}

#if defined(BAL_DBGLOG)
pid_t _bal_gettid(void)
{
    pid_t tid = 0;
# if defined(__WIN__)
    tid = (pid_t)GetCurrentThreadId();
# elif defined(__MACOS__)
    uint64_t tid64 = 0;
    int gettid = pthread_threadid_np(NULL, &tid64);
    if (0 != gettid)
        (void)_bal_handleerr(gettid);
    tid = (pid_t)tid64;
# elif defined(__linux__)
#  if (defined(__GLIBC__) && (__GLIBC__ >= 2 && __GLIBC_MINOR__ >= 30))
    tid = gettid();
#  else
    tid = syscall(SYS_gettid);
#  endif
# else
#  pragma message("warning: no implementation to get thread ID on this platform")
# endif
    return tid;
}
#endif

#if defined(__WIN__)
BOOL CALLBACK _bal_static_once_init_func(PINIT_ONCE ponce, PVOID param, PVOID* ctx)
{
    BAL_UNUSED(ponce);
    BAL_UNUSED(param);
    BAL_UNUSED(ctx);
# if defined(__HAVE_STDATOMICS__)
    atomic_init(&_bal_asyncselect_init, false);
    atomic_init(&_bal_as_container.die, false);
# else
    _bal_asyncselect_init = false;
    _bal_as_container.die = false;
# endif
    return TRUE;
}
#else
void _bal_static_once_init_func(void)
{
# if defined(__HAVE_STDATOMICS__)
    atomic_init(&_bal_asyncselect_init, false);
    atomic_init(&_bal_as_container.die, false);
# else
    _bal_asyncselect_init = false;
    _bal_as_container.die = false;
# endif
}
#endif
