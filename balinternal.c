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
#include "balinternal.h"
#include "bal.h"


/*─────────────────────────────────────────────────────────────────────────────╮
│                              Static globals                                  │
╰─────────────────────────────────────────────────────────────────────────────*/


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


/*─────────────────────────────────────────────────────────────────────────────╮
│                            Internal functions                                │
╰─────────────────────────────────────────────────────────────────────────────*/


bool _bal_init(void)
{
#if defined(__WIN__)
    WORD wVer  = MAKEWORD(2, 2);
    WSADATA wd = {0};

    if (0 != WSAStartup(wVer, &wd)) {
        _bal_handleerr(WSAGetLastError());
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
        BAL_ASSERT(cleanup);
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

int _bal_asyncselect(const bal_socket* s, bal_async_callback proc, uint32_t mask)
{
    if (!_bal_get_boolean(&_bal_asyncselect_init)) {
        BAL_ASSERT(!"async I/O not initialized");
        return BAL_FALSE;
    }

    if (_bal_get_boolean(&_bal_as_container.die)) {
        _bal_dbglog("async I/O shutting down; ignoring");
        return BAL_FALSE;
    }

    if (!s) {
        BAL_ASSERT(!"null bal_socket");
        return BAL_FALSE;
    }

    if (!proc && 0u != mask) {
        BAL_ASSERT(!"null proc with non-zero mask");
        return BAL_FALSE;
    }

    int r = BAL_FALSE;
    if (_bal_mutex_lock(&_bal_as_container.mutex)) {
        if (0u == mask) {
            bal_selectdata* d = NULL;
            bool success      = _bal_list_find(_bal_as_container.lst, s->sd, &d);
            BAL_ASSERT(NULL != d);
            if (success) {
                if (_bal_defer_remove_socket(d)) {
                    r = BAL_TRUE;
                    _bal_dbglog("added socket "BAL_SOCKET_SPEC" to defer"
                                " remove list", s->sd);
                } else {
                    BAL_ASSERT(!"failed to add socket to defer remove list");
                }
            } else {
                _bal_dbglog("warning: socket "BAL_SOCKET_SPEC" not in list;"
                            " ignoring", s->sd);
            }
        } else {
            bal_selectdata* d = NULL;
            if (_bal_list_find(_bal_as_container.lst, s->sd, &d)) {
                BAL_ASSERT(NULL != d);
                if (d) {
                    d->mask = mask;
                    d->proc = proc;
                    r       = BAL_TRUE;
                    _bal_dbglog("updated socket "BAL_SOCKET_SPEC, s->sd);
                }
            } else {
                d = calloc(1, sizeof(bal_selectdata));
                BAL_ASSERT(NULL != d);
                if (d) {
                    bool success = false;
                    if (BAL_FALSE == bal_setiomode(s, true)) {
                        _bal_handleerr(errno);
                    } else {
                        d->mask  = mask;
                        d->proc  = proc;
                        d->s     = (bal_socket*)s;
                        d->s->_f = 0u;
                        success  = _bal_defer_add_socket(d);
                        r        = success ? BAL_TRUE : BAL_FALSE;
                    }
                    if (success) {
                        _bal_dbglog("added socket "BAL_SOCKET_SPEC" to defer"
                                    " add list (mask = %08x, data = %p)", s->sd,
                                    d->mask, (uintptr_t)d);
                    } else {
                        _bal_safefree(&d);
                        BAL_ASSERT(!"failed to add socket to defer add list");
                    }
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
        &_bal_as_container.lst,
        &_bal_as_container.lst_add,
        &_bal_as_container.lst_rem
    };

    bool create = false;
    for (size_t n = 0; n < bal_countof(lists); n++) {
        create = _bal_list_create(lists[n]);
        BAL_ASSERT(create);

        if (!create) {
            (void)_bal_handleerr(errno);
            _bal_dbglog("error: failed to create list(s)");
            return false;
        }
    }

    bal_mutex* mutexes[] = {
        &_bal_as_container.mutex,
        &_bal_as_container.s_mutex
    };

    for (size_t n = 0; n < bal_countof(mutexes); n++) {
        create = _bal_mutex_create(mutexes[n]);
        BAL_ASSERT(create);

        if (!create) {
            _bal_dbglog("error: failed to create mutex(es)");
            return false;
        }
    }

    bal_condition* conditions[] = {
        &_bal_as_container.s_cond
    };

    for (size_t n = 0; n < bal_countof(conditions); n++) {
        create = _bal_cond_create(conditions[n]);
        BAL_ASSERT(create);

        if (!create) {
            _bal_dbglog("error: failed to create condition variable(s)");
            return false;
        }
    }

    struct {
        bal_thread* thread;
        bal_thread_func func;
    } threads[] = {
        {
            &_bal_as_container.thread,
            &_bal_eventthread
        },
        {
            &_bal_as_container.s_thread,
            &_bal_syncthread
        }
    };

    for (size_t n = 0; n < bal_countof(threads); n++) {
#if defined(__WIN__)
        *threads[n].thread = _beginthreadex(NULL, 0u, threads[n].func,
            &_bal_as_container, 0u, NULL);
        BAL_ASSERT(0ull != *threads[n].thread);

        if (0ull == *threads[n].thread) {
            (void)_bal_handleerr(errno);
            _bal_dbglog("error: failed to create thread(s)");
            return false;
        }
#else
        int op = pthread_create(threads[n].thread, NULL, threads[n].func,
            &_bal_as_container);
        BAL_ASSERT(0 == op);

        if (0 != op) {
            (void)_bal_handleerr(op);
            _bal_dbglog("error: failed to create thread(s)");
            return false;
        }
#endif
    }

    _bal_set_boolean(&_bal_asyncselect_init, true);
    _bal_dbglog("async I/O initialized");

    return true;
}

bool _bal_cleanupasyncselect(void)
{
    _bal_set_boolean(&_bal_as_container.die, true);

    bal_thread* threads[] = {
        &_bal_as_container.thread,
        &_bal_as_container.s_thread
    };

    _bal_dbglog("joining %zu threads...", bal_countof(threads));
    for (size_t n = 0; n < bal_countof(threads); n++) {
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
        _bal_as_container.lst,
        _bal_as_container.lst_add,
        _bal_as_container.lst_rem
    };

    for (size_t n = 0; n < bal_countof(lists); n++) {
        /* for the main list, some heap data may still be present;
         * the other lists contain the same pointers, so it's not necessary to
         * process them as well. */
        if (0 == n) {
            if (!_bal_list_empty(lists[n])) {
                /* iterate and free allocated data. */
                bal_descriptor sd  = 0;
                bal_selectdata* d  = NULL;

                _bal_dbglog("list %zu is not empty; freeing socket data...", n);

                _bal_list_reset_iterator(lists[n]);
                while (_bal_list_iterate(lists[n], &sd, &d)) {
                    _bal_dbglog("freeing data for socket "BAL_SOCKET_SPEC" (%p)",
                        sd, (uintptr_t)d);
                    _bal_safefree(&d);
                }
            } else {
                _bal_dbglog("list %zu is empty; no socket data to free", n);
            }
        }

        bool destroy = _bal_list_destroy(&lists[n]);
        BAL_ASSERT(destroy);
        cleanup &= destroy;
    }

    bal_mutex* mutexes[] = {
        &_bal_as_container.mutex,
        &_bal_as_container.s_mutex
    };

    for (size_t n = 0; n < bal_countof(mutexes); n++) {
        bool destroy = _bal_mutex_destroy(mutexes[n]);
        BAL_ASSERT(destroy);
        cleanup &= destroy;
    }

    bal_condition* conditions[] = {
        &_bal_as_container.s_cond
    };

    for (size_t n = 0; n < bal_countof(conditions); n++) {
        bool destroy = _bal_cond_destroy(conditions[n]);
        BAL_ASSERT(destroy);
        cleanup &= destroy;
    }

    _bal_set_boolean(&_bal_asyncselect_init, false);
    _bal_dbglog("async I/O cleaned up %s", cleanup ?
        "successfully" : "with errors");

    return cleanup;
}

bool _bal_defer_add_socket(bal_selectdata* d)
{
    bool retval = false;

    if (d) {
        bool locked = _bal_mutex_lock(&_bal_as_container.s_mutex);
        BAL_ASSERT(locked);

        if (locked) {
            retval = _bal_list_add(_bal_as_container.lst_add, d->s->sd, d) &&
                _bal_cond_broadcast(&_bal_as_container.s_cond);
            bool unlocked = _bal_mutex_unlock(&_bal_as_container.s_mutex);
            BAL_ASSERT_UNUSED(unlocked, unlocked);
        }
    }

    return retval;
}

bool _bal_defer_remove_socket(bal_selectdata* d)
{
    bool retval = false;

    if (d) {
        bool locked = _bal_mutex_lock(&_bal_as_container.s_mutex);
        BAL_ASSERT(locked);

        if (locked) {
            retval = _bal_list_add(_bal_as_container.lst_rem, d->s->sd, d) &&
                _bal_cond_broadcast(&_bal_as_container.s_cond);
            bool unlocked = _bal_mutex_unlock(&_bal_as_container.s_mutex);
            BAL_ASSERT_UNUSED(unlocked, unlocked);
        }
    }

    return retval;
}

int _bal_getaddrinfo(int f, int af, int st, const char* host, const char* port,
    bal_addrinfo* res)
{
    int r = BAL_FALSE;

    if (_bal_validstr(host) && res) {
        if (host) {
            struct addrinfo hints = {0};

            hints.ai_flags    = f;
            hints.ai_family   = af;
            hints.ai_socktype = st;

            r = getaddrinfo(host, port, (const struct addrinfo*)&hints, &res->_ai);

            if (0 != r)
                res->_ai = NULL;
            res->_p = res->_ai;
        }
    }

    if (BAL_TRUE != r && BAL_FALSE != r) {
        _bal_dbglog("getaddrinfo() failed with error %d (%s)", r, gai_strerror(r));
        _bal_setlasterror(r);
        r = BAL_FALSE;
    }

    return r;
}

int _bal_getnameinfo(int f, const bal_sockaddr* in, char* host, char* port)
{
    int r = BAL_FALSE;

    if (in && host) {
        socklen_t inlen = BAL_SASIZE(*in);
        r = getnameinfo((const struct sockaddr*)in, inlen, host, NI_MAXHOST,
            port, NI_MAXSERV, f);
    }

    if (BAL_TRUE != r && BAL_FALSE != r) {
        _bal_dbglog("getnameinfo() failed with error %d (%s)", r, gai_strerror(r));
        _bal_setlasterror(r);
        r = BAL_FALSE;
    }

    return r;
}

const struct addrinfo* _bal_enumaddrinfo(bal_addrinfo* ai)
{
    const struct addrinfo* r = NULL;

    if (ai && ai->_ai) {
        if (ai->_p) {
            r      = ai->_p;
            ai->_p = ai->_p->ai_next;
        } else {
            ai->_p = ai->_ai;
        }
    }

    return r;
}

int _bal_aitoal(bal_addrinfo* in, bal_addrlist* out)
{
    int r = BAL_FALSE;

    if (in && in->_ai && out) {
        const struct addrinfo* ai = NULL;
        bal_addr** a              = &out->_a;
        r                         = BAL_TRUE;

        in->_p = in->_ai;

        while (NULL != (ai = _bal_enumaddrinfo(in))) {
            *a = calloc(1ul, sizeof(bal_addr));

            if (!*a) {
                r = BAL_FALSE;
                break;
            }

            memcpy(&(*a)->_sa, ai->ai_addr, ai->ai_addrlen);
            a = &(*a)->_n;
        }

        bal_resetaddrlist(out);
    }

    return r;
}

int _bal_retstr(char* out, const char* in, size_t destlen)
{
    int r = BAL_FALSE;

    strncpy(out, in, destlen - 1);
    out[destlen - 1] = '\0';
    r = BAL_TRUE;

    return r;
}

int _bal_haspendingconnect(const bal_socket* s)
{
    return (s && bal_isbitset(s->_f, BAL_F_PENDCONN)) ? BAL_TRUE : BAL_FALSE;
}

int _bal_islistening(const bal_socket* s)
{
    return (s && bal_isbitset(s->_f, BAL_F_LISTENING)) ? BAL_TRUE : BAL_FALSE;
}

int _bal_isclosedcircuit(const bal_socket* s)
{
    int r = BAL_FALSE;

    if (s) {
        unsigned char buf = '\0';
        int rcv = recv(s->sd, &buf, sizeof(unsigned char), MSG_PEEK);

        if (0 == rcv)
            r = BAL_TRUE;
        else if (-1 == rcv) {
#if defined(__WIN__)
            int error = WSAGetLastError();
            if (WSAENETDOWN == error     || WSAENOTCONN == error  ||
                WSAEOPNOTSUPP == error   || WSAESHUTDOWN == error ||
                WSAECONNABORTED == error || WSAECONNRESET == error)
                r = BAL_TRUE;
#else
            if (EBADF == errno || ENOTCONN == errno || ENOTSOCK == errno)
                r = BAL_TRUE;
#endif
        }
    }

    return r;
}

BALTHREAD _bal_eventthread(void* ctx)
{
    bal_as_container* asc = (bal_as_container*)ctx;
    BAL_ASSERT(NULL != asc);

    while (!_bal_get_boolean(&asc->die)) {
        if (_bal_mutex_lock(&asc->mutex)) {
            if (!_bal_list_empty(asc->lst)) {
                fd_set fd_read   = {0};
                fd_set fd_write  = {0};
                fd_set fd_except = {0};

                FD_ZERO(&fd_read);
                FD_ZERO(&fd_write);
                FD_ZERO(&fd_except);

                _bal_list_event_prepare_data epd = {
                    &fd_read,
                    &fd_write,
                    &fd_except,
                    0
                };

                bool iterate = _bal_list_iterate_func(asc->lst, &epd,
                    &__bal_list_event_prepare);
                BAL_ASSERT(iterate);

                struct timeval tv = {0, 0};
                int poll_res = select(epd.high_watermark + 1, &fd_read, &fd_write,
                    &fd_except, &tv);

                if (-1 != poll_res) {
                    _bal_dispatchevents(&fd_read, asc, BAL_S_READ);
                    _bal_dispatchevents(&fd_write, asc, BAL_S_WRITE);
                    _bal_dispatchevents(&fd_except, asc, BAL_S_EXCEPT);
                }
            }

            bool unlocked = _bal_mutex_unlock(&asc->mutex);
            BAL_ASSERT_UNUSED(unlocked, unlocked);
        }

        bal_yield_thread();
    }

#if defined(__WIN__)
    return 0u;
#else
    return NULL;
#endif
}

BALTHREAD _bal_syncthread(void* ctx)
{
    bal_as_container* asc = (bal_as_container*)ctx;
    BAL_ASSERT(NULL != asc);

    while (!_bal_get_boolean(&asc->die)) {
        if (_bal_mutex_lock(&asc->s_mutex)) {
            while (_bal_list_empty(asc->lst_add) && _bal_list_empty(asc->lst_rem) &&
                  !_bal_get_boolean(&asc->die)) {
#if !defined(__WIN__)
                /* seconds; absolute fixed time. */
                bal_wait wait = {time(NULL) + 1, 0};
#else
                /* msec; relative from now. */
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

        bal_yield_thread();
    }

#if defined(__WIN__)
    return 0u;
#else
    return NULL;
#endif
}

void _bal_dispatchevents(fd_set* set, bal_as_container* asc, uint32_t type)
{
    BAL_ASSERT(set && asc);
    if (set && asc) {
        _bal_list_dispatch_data ldd = {
            set,
            type
        };

        bool iterate = _bal_list_iterate_func(asc->lst, &ldd, &__bal_list_dispatch_events);
        BAL_ASSERT(iterate);
    }
}

bool _bal_list_create(bal_list** lst)
{
    bool retval = NULL != lst;

    if (retval) {
        *lst = calloc(1ul, sizeof(bal_list));
        retval = NULL != *lst;
    }

    return retval;
}

bool _bal_list_create_node(bal_list_node** node, bal_descriptor key, bal_selectdata* val)
{
    bool ok = NULL != node;

    if (ok) {
        *node = calloc(1ul, sizeof(bal_list_node));
        if (*node) {
            (*node)->key = key;
            (*node)->val = val;
        }
        ok = NULL != *node;
    }

    return ok;
}

bool _bal_list_add(bal_list* lst, bal_descriptor key, bal_selectdata* val)
{
    bool ok = NULL != lst;

    if (ok) {
        if (_bal_list_empty(lst)) {
            ok = _bal_list_create_node(&lst->head, key, val);
            lst->iter = lst->head;
        } else {
            bal_list_node* node = lst->head;
            while (node && node->next)
                node = node->next;

            ok = _bal_list_create_node(&node->next, key, val);
            if (ok)
                node->next->prev = node;
        }
    }

    return ok;
}

bool _bal_list_find(bal_list* lst, bal_descriptor key, bal_selectdata** val)
{
    bool ok = !_bal_list_empty(lst) && NULL != val;

    if (ok) {
        _bal_list_find_data lfd = { key, val, false };
        ok = _bal_list_iterate_func(lst, &lfd, &__bal_list_find_key) && lfd.found;
    }

    return ok;
}

bool _bal_list_empty(bal_list* lst)
{
    return !lst || !lst->head;
}

bool _bal_list_iterate(bal_list* lst, bal_descriptor* key, bal_selectdata** val)
{
    bool ok = !_bal_list_empty(lst) && NULL != key && NULL != val;

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
    if (NULL != lst)
        lst->iter = lst->head;
}

bool _bal_list_iterate_func(bal_list* lst, void* ctx, bal_list_iter_callback cb)
{
    bool ok = !_bal_list_empty(lst) && NULL != cb;

    if (ok) {
        size_t count        = 0ul;
        bal_list_node* node = lst->head;
        while (node) {
            count++;
            if (!cb(node->key, node->val, ctx))
                break;
            node = node->next;
        }
        ok = 0ul < count;
    }

    return ok;
}

bool _bal_list_remove(bal_list* lst, bal_descriptor key, bal_selectdata** val)
{
    bool ok = !_bal_list_empty(lst) && NULL != val;

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
    bool ok = NULL != lst && NULL != *lst;

    if (ok) {
        if (!_bal_list_empty(*lst))
            ok = _bal_list_remove_all(*lst);
        _bal_safefree(lst);
    }

    return ok;
}

bool _bal_list_destroy_node(bal_list_node** node)
{
    bool ok = NULL != node && NULL != *node;

    if (ok) {
        if ((*node)->prev)
            (*node)->prev->next = (*node)->next;

        if ((*node)->next)
            (*node)->next->prev = (*node)->prev;

        _bal_safefree(node);
    }

    return ok;
}

bool __bal_list_find_key(bal_descriptor key, bal_selectdata* val, void* ctx)
{
    _bal_list_find_data* lfd = (_bal_list_find_data*)ctx;
    if (key == lfd->key) {
        *lfd->val = val;
        lfd->found = true;
        return false;
    }
    return true;
}

bool __bal_list_dispatch_events(bal_descriptor key, bal_selectdata* val, void* ctx)
{
    _bal_list_dispatch_data* ldd = (_bal_list_dispatch_data*)ctx;
    if (0 != FD_ISSET(key, ldd->set)) {
        uint32_t event = 0u;
        bool snd       = false;

        switch (ldd->type) {
            case BAL_S_READ:
                if (bal_isbitset(val->mask, BAL_E_READ)) {
                    if (bal_isbitset(val->mask, BAL_S_LISTEN))
                        event = BAL_E_ACCEPT;
                    else if (BAL_TRUE == _bal_isclosedcircuit(val->s))
                        event = BAL_E_CLOSE;
                    else
                        event = BAL_E_READ;
                    snd = true;
                }
            break;
            case BAL_S_WRITE:
                if (bal_isbitset(val->mask, BAL_E_WRITE)) {
                    if (bal_isbitset(val->mask, BAL_S_CONNECT)) {
                        event = BAL_E_CONNECT;
                        val->mask &= ~BAL_S_CONNECT;
                    } else
                        event = BAL_E_WRITE;
                    snd = true;
                }
            break;
            case BAL_S_EXCEPT:
                if (bal_isbitset(val->mask, BAL_E_EXCEPTION)) {
                    if (bal_isbitset(val->mask, BAL_S_CONNECT)) {
                        event = BAL_E_CONNFAIL;
                        val->mask &= ~BAL_S_CONNECT;
                    } else
                        event = BAL_E_EXCEPTION;
                    snd = true;
                }
            break;
            default:
                BAL_ASSERT(!"invalid event type");
            break;
        }

        if (snd)
            val->proc(val->s, event);

        if (BAL_E_CLOSE == event && !bal_isbitset(val->mask, BAL_S_CLOSE)) {
            /* since we are actively iterating the linked list, we cannot remove
             * an entry here. instead, defer its removal by the sync thread. */
            bool added = _bal_defer_remove_socket(val);
            BAL_ASSERT_UNUSED(added, added);
            if (added) {
                val->mask |= BAL_S_CLOSE;
                _bal_dbglog("added socket "BAL_SOCKET_SPEC" to defer"
                             " remove list", key);
            } else {
                BAL_ASSERT(!"failed to add socket to defer remove list");
            }
        }
    }

    return true;
}

bool __bal_list_remove_entries(bal_descriptor key, bal_selectdata* val, void* ctx)
{
    bal_list* lst = (bal_list*)ctx;

    BAL_UNUSED(val);

    bal_selectdata* d = NULL;
    bool removed = _bal_list_remove(lst, key, &d);
    BAL_ASSERT_UNUSED(removed, removed);
    _bal_safefree(&d);

    if (removed)
        _bal_dbglog("removed socket "BAL_SOCKET_SPEC" from list", key);

    return true;
}

bool __bal_list_add_entries(bal_descriptor key, bal_selectdata* val, void* ctx)
{
    bal_list* lst = (bal_list*)ctx;
    bool added    = _bal_list_add(lst, key, val);
    BAL_ASSERT_UNUSED(added, added);

    if (added)
        _bal_dbglog("added socket "BAL_SOCKET_SPEC" to list", key);

    return true;
}

bool __bal_list_event_prepare(bal_descriptor key, bal_selectdata* val, void* ctx)
{
    _bal_list_event_prepare_data* epd = (_bal_list_event_prepare_data*)ctx;

    if (BAL_TRUE == _bal_haspendingconnect(val->s)) {
        val->mask |= BAL_S_CONNECT;
        val->s->_f &= ~BAL_F_PENDCONN;
    }

    if (BAL_TRUE == _bal_islistening(val->s)) {
        val->mask |= BAL_S_LISTEN;
        val->s->_f &= ~BAL_F_LISTENING;
    }

    if (key > epd->high_watermark)
        epd->high_watermark = key;

    FD_SET(key, epd->fd_read);
    FD_SET(key, epd->fd_write);
    FD_SET(key, epd->fd_except);

    return true;
}

#if !defined(__WIN__) /* pthread mutex implementation. */
bool _bal_mutex_create(bal_mutex* mutex) {
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

bool _bal_mutex_lock(bal_mutex* mutex) {
    if (_bal_validptr(mutex)) {
        int op = pthread_mutex_lock(mutex);
        return 0 == op ? true : _bal_handleerr(op);
    }

    return false;
}

bool _bal_mutex_trylock(bal_mutex* mutex) {
    if (_bal_validptr(mutex)) {
        int op = pthread_mutex_trylock(mutex);
        return 0 == op ? true : EBUSY != op ? _bal_handleerr(op) : false;
    }

    return false;
}

bool _bal_mutex_unlock(bal_mutex* mutex) {
    if (_bal_validptr(mutex)) {
        int op = pthread_mutex_unlock(mutex);
        return 0 == op ? true : _bal_handleerr(op);
    }

    return false;
}

bool _bal_mutex_destroy(bal_mutex* mutex) {
    if (_bal_validptr(mutex)) {
        int op = pthread_mutex_destroy(mutex);
        return 0 == op ? true : _bal_handleerr(op);
    }

    return false;
}
#else /* __WIN__ */
bool _bal_mutex_create(bal_mutex* mutex) {
    if (_bal_validptr(mutex)) {
        InitializeCriticalSection(mutex);
        return true;
    }

    return false;
}

bool _bal_mutex_lock(bal_mutex* mutex) {
    if (_bal_validptr(mutex)) {
        EnterCriticalSection(mutex);
        return true;
    }

    return false;
}

bool _bal_mutex_trylock(bal_mutex* mutex) {
    if (_bal_validptr(mutex))
        return FALSE != TryEnterCriticalSection(mutex);

    return false;
}

bool _bal_mutex_unlock(bal_mutex* mutex) {
    if (_bal_validptr(mutex)) {
        LeaveCriticalSection(mutex);
        return true;
    }

    return false;
}

bool _bal_mutex_destroy(bal_mutex* mutex) {
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
            ? true : _bal_handleerr(GetLastError());

    return valid;
}
#endif

#if defined(__HAVE_STDATOMICS__)
bool _bal_get_boolean(atomic_bool* boolean)
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
bool _bal_get_boolean(bool* boolean)
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

bool _bal_once(bal_once* once, bal_once_fn func)
{
#if defined(__WIN__)
    return FALSE != InitOnceExecuteOnce(once, func, NULL, NULL);
#else
    return 0 == pthread_once(once, func);
#endif
}

pid_t _bal_gettid(void)
{
    pid_t tid = 0;
#if defined(__WIN__)
    tid = (pid_t)GetCurrentThreadId();
#elif defined(__MACOS__)
    uint64_t tid64 = 0;
    int gettid = pthread_threadid_np(NULL, &tid64);
    if (0 != gettid)
        (void)_bal_handleerr(gettid);
    tid = (pid_t)tid64;
#elif defined(__linux__)
# if (defined(__GLIBC__) && (__GLIBC__ >= 2 && __GLIBC_MINOR__ >= 30))
    tid = gettid();
# else
    tid = syscall(SYS_gettid);
# endif
#else
    _bal_dbglog("warning: no implementation to get thread ID on this platform");
#endif
    return tid;
}

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

void _bal_socket_tostr(const bal_socket* s, char buf[256])
{
    if (s) {
        (void)snprintf(buf, 256, "%"PRIxPTR":\n{\n\tsd = "BAL_SOCKET_SPEC"\n\t"
            "af = %d\n\tst = %d\n\tpf = %d\n\t_f = 0x%08x\n}", (uintptr_t)s,
            s->sd, s->af, s->st, s->pf, s->_f);
    } else {
        (void)snprintf(buf, 256, "<null>");
    }
}
