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
#include "bal/state.h"
#include "bal.h"

/******************************************************************************\
 *                             Internal Functions                             *
\******************************************************************************/

bool _bal_sanity(void)
{
#if defined(__HAVE_STDATOMICS__)
    uint_fast32_t magic = atomic_load(&_bal_state.magic);
#else
    uint_fast32_t magic = _bal_state.magic;
#endif

    BAL_ASSERT(0U == magic || BAL_MAGIC == magic);
    if (BAL_MAGIC != magic)
        return _bal_seterror(_BAL_E_NOTINIT);

    return true;
}

bool _bal_init_asyncpoll(void)
{
    if (_bal_get_boolean(&_bal_async_poll_init))
        return _bal_seterror(_BAL_E_ASDUPEINIT);

    bool init = _bal_list_create(&_bal_as_container.lst);
    if (!init) {
        _bal_dbglog("error: failed to create list");
        return _bal_handlelasterr();
    }

    _bal_andeql(init, _bal_mutex_create(&_bal_as_container.mutex));
    if (!init) {
        _bal_dbglog("error: failed to create mutex(es)");
        return false;
    }

    struct {
        bal_thread* thread;
        bal_thread_cb proc;
    } threads[] = {
        {
            &_bal_as_container.thread,
            &_bal_eventthread
        }
    };

    for (size_t n = 0; n < _bal_countof(threads); n++) {
#if defined(__WIN__)
        *threads[n].thread = _beginthreadex(NULL, 0U, threads[n].proc, NULL,
            0U, NULL);
        BAL_ASSERT(0ULL != *threads[n].thread);

        if (0ULL == *threads[n].thread)
            _bal_andeql(init, _bal_handlelasterr());
#else
        int op = pthread_create(threads[n].thread, NULL, threads[n].proc, NULL);
        BAL_ASSERT(0 == op);
        _bal_andeql(init, 0 == op);

        if (0 != op)
            (void)_bal_handleerr(op);
#endif
    }

    _bal_set_boolean(&_bal_async_poll_init, init);
    _bal_set_boolean(&_bal_as_container.die, !init);
    _bal_dbglog("async I/O initialization %s", init ? "succeeded" : "failed");

    return init;
}

bool _bal_cleanup_asyncpoll(void)
{
    if (!_bal_get_boolean(&_bal_async_poll_init))
        return _bal_seterror(_BAL_E_ASNOTINIT);

    _bal_set_boolean(&_bal_as_container.die, true);
    _bal_set_boolean(&_bal_async_poll_init, false);

    bal_thread* threads[] = {
        &_bal_as_container.thread
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

    bool cleanup       = true;
    bal_descriptor key = 0;
    bal_socket* val    = NULL;

    _bal_list_reset_iterator(_bal_as_container.lst);
    while (_bal_list_iterate(_bal_as_container.lst, &key, &val)) {
        _bal_dbglog("warning: dangling bal_socket "BAL_SOCKET_SPEC" (%p)",
            key, val);
    }

    bool destroy = _bal_list_destroy(&_bal_as_container.lst);
    BAL_ASSERT(destroy);
    _bal_andeql(cleanup, destroy);

    destroy = _bal_mutex_destroy(&_bal_as_container.mutex);
    BAL_ASSERT(destroy);
    _bal_andeql(cleanup, destroy);

    _bal_dbglog("async I/O clean up %s", cleanup ? "succeeded" : "failed");

    return cleanup;
}

bool _bal_get_addrinfo(int flags, int addr_fam, int type, const char* host,
    const char* port, struct addrinfo** res)
{
    bool retval = false;

    if ((_bal_okstr(host) || _bal_okstr(port)) && _bal_okptrptr(res)) {
        struct addrinfo hints = {0};

        hints.ai_flags    = flags;
        hints.ai_family   = addr_fam;
        hints.ai_socktype = type;

        int get = getaddrinfo(host, port, (const struct addrinfo*)&hints, res);
        if (0 != get)
            _bal_handlegaierr(get);
        retval = 0 == get;
    }

    return retval;
}

bool _bal_getnameinfo(int flags, const bal_sockaddr* in, char* host, char* port)
{
    bool retval = false;

    if (_bal_okptr(in) && _bal_okptr(host)) {
        int get = getnameinfo((const struct sockaddr*)in, _BAL_SASIZE(*in), host,
            NI_MAXHOST, port, NI_MAXSERV, flags);
        if (0 != get)
            _bal_handlegaierr(get);
        retval = 0 == get;
    }

    return retval;
}

bool _bal_is_pending_conn(const bal_socket* s)
{
    return _bal_oksock(s) && bal_isbitset(s->state.bits, BAL_S_CONNECT);
}

bool _bal_is_closed_conn(const bal_socket* s)
{
#if defined(__WIN__)
    /* Windows doesn't have MSG_DONTWAIT, so this method is unacceptable for that
     * platform. If the socket were not asynchronous, this could cause an
     * indefinite hang. */
    BAL_UNUSED(s);
    return false;
#else
    if (!_bal_oksock(s))
        return true;

    int buf = 0;
    ssize_t rcv = bal_recv(s, &buf, sizeof(int), MSG_PEEK | MSG_DONTWAIT);
    if (0 == rcv)
        return true;
    if (-1 == rcv) {
        int error = errno;
        if (ENETDOWN     == error || ENOTCONN     == error ||
            ECONNREFUSED == error || ESHUTDOWN    == error ||
            ECONNABORTED == error || ECONNRESET   == error ||
            ENETUNREACH  == error || ENETRESET    == error ||
            EHOSTDOWN    == error || EHOSTUNREACH == error)
            return true;
    }

    return false;
#endif
}

uint32_t _bal_on_pending_conn_io(bal_socket* s, uint32_t* events)
{
    uint32_t retval = 0U;

    if (_bal_oksock(s) && _bal_okptr(events))
    {
        if (bal_isbitset(*events, BAL_EVT_CLOSE) ||
            bal_isbitset(*events, BAL_EVT_ERROR)) {
            bal_setbitslow(events, BAL_EVT_ERROR);
            _bal_handlesockerr(s);
            retval = BAL_EVT_CONNFAIL;
        } else {
            retval = BAL_EVT_CONNECT;
        }

        bal_setbitslow(&s->state.mask, BAL_EVT_WRITE);
        bal_setbitslow(&s->state.bits, BAL_S_CONNECT);
    }

    return retval;
}

uint32_t _bal_pollflags_to_events(short flags)
{
    uint32_t retval = 0U;

    if (bal_isbitset(flags, POLLRDNORM))
        bal_setbitshigh(&retval, BAL_EVT_READ);

    if (bal_isbitset(flags, POLLWRNORM))
        bal_setbitshigh(&retval, BAL_EVT_WRITE);

    if (bal_isbitset(flags, POLLRDBAND))
        bal_setbitshigh(&retval, BAL_EVT_OOBREAD);

#if !defined(__WIN__)
    if (bal_isbitset(flags, POLLWRBAND))
        bal_setbitshigh(&retval, BAL_EVT_OOBWRITE);

    if (bal_isbitset(flags, POLLPRI))
        bal_setbitshigh(&retval, BAL_EVT_PRIORITY);
#endif

    if (bal_isbitset(flags, POLLHUP))
        bal_setbitshigh(&retval, BAL_EVT_CLOSE);

#if defined(__HAVE_POLLRDHUP__)
    if (bal_isbitset(flags, POLLRDHUP))
        bal_setbitshigh(&retval, BAL_EVT_CLOSE);
#endif

    if (bal_isbitset(flags, POLLERR))
        bal_setbitshigh(&retval, BAL_EVT_ERROR);

    if (bal_isbitset(flags, POLLNVAL))
        bal_setbitshigh(&retval, BAL_EVT_INVALID);

    return retval;
}

short _bal_mask_to_pollflags(uint32_t mask)
{
    short retval = 0;

    if (bal_isbitset(mask, BAL_EVT_READ))
        bal_setbitshigh(&retval, POLLRDNORM);

    if (bal_isbitset(mask, BAL_EVT_WRITE))
        bal_setbitshigh(&retval, POLLWRNORM);

    if (bal_isbitset(mask, BAL_EVT_OOBREAD))
        bal_setbitshigh(&retval, POLLRDBAND);

#if !defined(__WIN__)
    if (bal_isbitset(mask, BAL_EVT_OOBWRITE))
        bal_setbitshigh(&retval, POLLWRBAND);

    if (bal_isbitset(mask, BAL_EVT_PRIORITY))
        bal_setbitshigh(&retval, POLLPRI);
#endif

#if defined(__HAVE_POLLRDHUP__)
    if (bal_isbitset(mask, BAL_EVT_CLOSE))
        bal_setbitshigh(&retval, POLLRDHUP);
#endif

    return retval;
}

bal_threadret _bal_eventthread(void* ctx)
{
    BAL_UNUSED(ctx);
    static const int poll_timeout = 500;

    while (!_bal_get_boolean(&_bal_as_container.die)) {
        size_t count       = 0UL;
#if defined(__WIN__)
        WSAPOLLFD* fds     = NULL;
#else
        struct pollfd* fds = NULL;
#endif
        _BAL_MUTEX_COUNTER_INIT(eventthread);
        _BAL_LOCK_MUTEX(&_bal_as_container.mutex, eventthread);

        count = _bal_list_count(_bal_as_container.lst);
        if (count > 0UL) {
            fds = calloc(count, sizeof(struct pollfd));
            BAL_ASSERT(NULL != fds);

            if (_bal_okptrnf(fds)) {
                size_t offset      = 0;
                bal_descriptor key = 0;
                bal_socket* val    = NULL;

                _bal_list_reset_iterator(_bal_as_container.lst);
                while (_bal_list_iterate(_bal_as_container.lst, &key, &val)) {
                    fds[offset].fd     = key;
                    fds[offset].events = _bal_mask_to_pollflags(val->state.mask);
                    offset++;
                }

                /* relinquish the mutex during poll; this gives other threads
                 * a chance to obtain the lock and do some work. */
                _BAL_UNLOCK_MUTEX(&_bal_as_container.mutex, eventthread);
#if defined(__WIN__)
                int res = WSAPoll(fds, (nfds_t)count, poll_timeout);
#else
                int res = poll(fds, (nfds_t)count, poll_timeout);
#endif
                /* get the mutex back. */
                _BAL_LOCK_MUTEX(&_bal_as_container.mutex, eventthread);

                if (res > 0) {
                    for (size_t n = 0UL; n < count; n++) {
                        bal_socket* s = NULL;
                        bool found    = _bal_list_find(_bal_as_container.lst,
                            fds[n].fd, &s);

                        if (found && _bal_oksock(s)) {
                            uint32_t events = _bal_pollflags_to_events(fds[n].revents);
                            if (0U != events)
                                _bal_dispatch_events(fds[n].fd, s, events);
                        }
                    }
                } else if (-1 == res) {
                    _bal_handlelasterr();
                }

                _bal_safefree(&fds);
            }
         }

        _BAL_UNLOCK_MUTEX(&_bal_as_container.mutex, eventthread);
        _BAL_MUTEX_COUNTER_CHECK(eventthread);

        if (0 == count)
            bal_sleep_msec(100); // TODO: better alternative than sleeping here?
        bal_thread_yield();
    }

#if defined(__WIN__)
    return 0U;
#else
    return NULL;
#endif
}

void _bal_dispatch_events(bal_descriptor sd, bal_socket* s, uint32_t events)
{
    BAL_ASSERT(NULL != s);
    if (!_bal_okptr(s)) {
        return;
    }

    uint32_t _events = 0U;

#if defined(BAL_DBGLOG_ASYNC_IO)
    _bal_dbglog("events %08"PRIx32" for socket "BAL_SOCKET_SPEC " (mask = %08"
        PRIx32")", events, sd, s->state.mask);
#endif

    if (bal_isbitset(events, BAL_EVT_READ) && bal_bitsinmask(s, BAL_EVT_READ)) {
        if (bal_is_listening(s)) {
            bal_setbitshigh(&_events, BAL_EVT_ACCEPT);
        } else if (_bal_is_pending_conn(s)) {
            _events |= _bal_on_pending_conn_io(s, &events);
#if !defined(__HAVE_POLLRDHUP__)
        } else if (_bal_is_closed_conn(s)) {
            /* Some platforms insist upon spamming read events if the peer
             * shuts down their end of the connection, presumably prodding you
             * to do a read, get a zero return value, and then close the socket.
             * Just do that here, and translate it to a close event instead. */
            bal_setbitshigh(&_events, BAL_EVT_CLOSE);
#endif
        } else {
            bal_setbitshigh(&_events, BAL_EVT_READ);
        }
    }

    if (bal_isbitset(events, BAL_EVT_OOBREAD) && bal_bitsinmask(s, BAL_EVT_OOBREAD))
        bal_setbitshigh(&_events, BAL_EVT_OOBREAD);

    if (bal_isbitset(events, BAL_EVT_WRITE) && bal_bitsinmask(s, BAL_EVT_WRITE)) {
        if (_bal_is_pending_conn(s)) {
            _events |= _bal_on_pending_conn_io(s, &events);
        } else {
            bal_setbitshigh(&_events, BAL_EVT_WRITE);
        }
    }

    if (bal_isbitset(events, BAL_EVT_OOBWRITE) && bal_bitsinmask(s, BAL_EVT_OOBWRITE))
        bal_setbitshigh(&_events, BAL_EVT_OOBWRITE);

    if (bal_isbitset(events, BAL_EVT_CLOSE) && bal_bitsinmask(s, BAL_EVT_CLOSE))
        bal_setbitshigh(&_events, BAL_EVT_CLOSE);

    if (bal_isbitset(events, BAL_EVT_PRIORITY) && bal_bitsinmask(s, BAL_EVT_PRIORITY))
        bal_setbitshigh(&_events, BAL_EVT_PRIORITY);

    if (bal_isbitset(events, BAL_EVT_ERROR) && bal_bitsinmask(s, BAL_EVT_ERROR))
        bal_setbitshigh(&_events, BAL_EVT_ERROR);

    if (bal_isbitset(events, BAL_EVT_INVALID) && bal_bitsinmask(s, BAL_EVT_INVALID))
        bal_setbitshigh(&_events, BAL_EVT_INVALID);

    bool closed  = bal_isbitset(events, BAL_EVT_CLOSE);
    bool invalid = bal_isbitset(events, BAL_EVT_INVALID);

    if (0U != _events && _bal_okptr(s->state.proc))
        s->state.proc(s, _events);

    if (closed || invalid) {
        /* if the callback did the right thing, it has called bal_close and
         * possibly bal_sock_destroy. if it didn't call the latter, the socket
         * still resides in the list. presume that the callback is behaving
         * properly–don't free the socket, but remove it from the list. */
        bal_socket* d = NULL;
        bool removed  = _bal_list_remove(_bal_as_container.lst, sd, &d);

        if (removed) {
            _bal_dbglog("removed socket "BAL_SOCKET_SPEC" (%p) from list"
                        " (closed/invalid)", sd, s);
        } else {
            _bal_dbglog("socket "BAL_SOCKET_SPEC" destroyed by event"
                        " handler (closed/invalid)", sd);
        }
    }
}

bool _bal_list_create(bal_list** lst)
{
    bool retval = _bal_okptr(lst);

    if (retval) {
        *lst = calloc(1UL, sizeof(bal_list));
        retval = NULL != *lst;
    }

    return retval;
}

bool _bal_list_create_node(bal_list_node** node, bal_descriptor key,
    bal_socket* val)
{
    bool ok = _bal_okptrptr(node);

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
    bool ok = _bal_okptr(lst);

    if (ok) {
        if (_bal_list_empty(lst)) {
            ok = _bal_list_create_node(&lst->head, key, val);
            lst->iter = lst->head;
        } else {
            bal_list_node* node = lst->head;
            while (node && node->next)
                node = node->next;
            ok = NULL != node && _bal_list_create_node(&node->next, key, val) &&
                 NULL != node->next;
            if (ok)
                node->next->prev = node;
        }
    }

    return ok;
}

bool _bal_list_find(bal_list* lst, bal_descriptor key, bal_socket** val)
{
    bool ok = !_bal_list_empty(lst) && _bal_okptr(val);

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
    bool ok = !_bal_list_empty(lst) && _bal_okptr(key) && _bal_okptr(val);

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
    if (_bal_okptr(lst))
        lst->iter = lst->head;
}

bool _bal_list_iterate_func(bal_list* lst, void* ctx, bal_list_iter_cb cb)
{
    bool ok = !_bal_list_empty(lst) && _bal_okptr(cb);

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
    bool ok = !_bal_list_empty(lst) && _bal_okptr(val);

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
        _bal_andeql(ok, found);
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
            _bal_andeql(ok , _bal_list_destroy_node(&node));
            node = next;
        }

        lst->head = NULL;
    }

    return ok;
}

bool _bal_list_destroy(bal_list** lst)
{
    bool ok = _bal_okptrptr(lst) && _bal_okptr(*lst);

    if (ok) {
        if (!_bal_list_empty(*lst))
            ok = _bal_list_remove_all(*lst);
        _bal_safefree(lst);
    }

    return ok;
}

bool _bal_list_destroy_node(bal_list_node** node)
{
    bool ok = _bal_okptrptr(node) && _bal_okptr(node);

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

#if !defined(__WIN__) /* pthread mutex implementation. */
bool _bal_mutex_create(bal_mutex* mutex)
{
    if (_bal_okptr(mutex)) {
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
    if (_bal_okptr(mutex)) {
        int op = pthread_mutex_lock(mutex);
        return 0 == op ? true : _bal_handleerr(op);
    }
    return false;
}

bool _bal_mutex_unlock(bal_mutex* mutex)
{
    if (_bal_okptr(mutex)) {
        int op = pthread_mutex_unlock(mutex);
        return 0 == op ? true : _bal_handleerr(op);
    }
    return false;
}

bool _bal_mutex_destroy(bal_mutex* mutex)
{
    if (_bal_okptr(mutex)) {
        int op = pthread_mutex_destroy(mutex);
        return 0 == op ? true : _bal_handleerr(op);
    }
    return false;
}
#else /* __WIN__ */
bool _bal_mutex_create(bal_mutex* mutex)
{
    if (_bal_okptr(mutex)) {
        InitializeCriticalSection(mutex);
        return true;
    }
    return false;
}

bool _bal_mutex_lock(bal_mutex* mutex)
{
    if (_bal_okptr(mutex)) {
        EnterCriticalSection(mutex);
        return true;
    }
    return false;
}

bool _bal_mutex_trylock(bal_mutex* mutex)
{
    if (_bal_okptr(mutex))
        return FALSE != TryEnterCriticalSection(mutex);
    return false;
}

bool _bal_mutex_unlock(bal_mutex* mutex)
{
    if (_bal_okptr(mutex)) {
        LeaveCriticalSection(mutex);
        return true;
    }
    return false;
}

bool _bal_mutex_destroy(bal_mutex* mutex)
{
    if (_bal_okptr(mutex)) {
        DeleteCriticalSection(mutex);
        return true;
    }
    return false;
}
#endif /* !__WIN__ */

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

bool _bal_once(bal_once* once, bal_once_fn func)
{
#if defined(__WIN__)
    return FALSE != InitOnceExecuteOnce(once, func, NULL, NULL);
#else
    return 0 == pthread_once(once, func);
#endif
}

bool _bal_addrinfo_to_addrlist(struct addrinfo* ai, bal_addrlist* out)
{
    if (_bal_okptr(ai) && _bal_okptr(out)) {
        struct addrinfo* cur = ai;
        bal_addr** a         = &out->addr;

        do {
            *a = calloc(1UL, sizeof(bal_addr));
            if (!_bal_okptrnf(*a))
                return _bal_handlelasterr();

            memcpy(&(*a)->addr, cur->ai_addr, cur->ai_addrlen);

            a   = &(*a)->next;
            cur = cur->ai_next;
        } while (NULL != cur);

        bal_reset_addrlist(out);
        return true;
    }

    return false;
}

void _bal_strcpy(char* dest, size_t destsz, const char* src, size_t srcsz)
{
    if (_bal_okptr(dest) && _bal_oklen(destsz) && _bal_okstr(src) &&
        _bal_oklen(srcsz)) {
        /* Use strncpy_s if it's available (will likely only ever be on Windows. */
#if defined(__HAVE_STDC_SECURE_OR_EXT1__)
        errno_t copy = strncpy_s(dest, destsz, src, srcsz);
        BAL_ASSERT_UNUSED(copy, 0 == copy);
#elif defined(__HAVE_LIBC_STRLCPY__)
        /* Use strlcpy if it's available (it is on at least BSD-derivatives. */
        size_t copy = strlcpy(dest, src, destsz);
        BAL_ASSERT_UNUSED(copy, copy <= destsz);
        BAL_UNUSED(srcsz);
#else
        /* We'll do it live. It's better than using strncpy and hearing about
         * 'unsafe' code from certain static analyzers. */
        const char* src_cursor = src;
        char* dest_cursor = dest;

        while (*src_cursor != '\0' && (uintptr_t)(src_cursor - src) < destsz - 1 &&
               (uintptr_t)(src_cursor - src) < srcsz)
            *(dest_cursor++) = *(src_cursor++);
        *dest_cursor = '\0';
#endif
    }
}

void _bal_socket_print(const bal_socket* s)
{
    if (_bal_okptr(s)) {
        printf("%p:\n{\n  sd = "BAL_SOCKET_SPEC"\n  addr_fam = %d\n  type = %d\n"
               "  proto = %d\nstate =\n  {\n  mask = %"PRIx32"\n  proc = %"
               PRIxPTR"\n  }\n}\n", (void*)s, s->sd, s->addr_fam, s->type, s->proto,
               s->state.mask, (uintptr_t)s->state.proc);
    } else {
        printf("<null>\n");
    }
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
# elif (defined(__BSD__) && !defined(__NetBSD__) && !defined(__OpenBSD__)) || \
        defined(__DragonFly_getthreadid__)
    tid = (pid_t)pthread_getthreadid_np();
# elif defined(__OpenBSD__)
    tid = (pid_t)getthrid();
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
#else
void _bal_static_once_init_func(void)
{
#endif
    bool create = _bal_mutex_create(&_bal_state.mutex);
    BAL_ASSERT_UNUSED(create, create);
#if defined(__HAVE_STDATOMICS__)
    atomic_init(&_bal_state.magic, 0U);
    atomic_init(&_bal_async_poll_init, false);
    atomic_init(&_bal_as_container.die, false);
#else
    _bal_state.magic      = 0U;
    _bal_async_poll_init  = false;
    _bal_as_container.die = false;
#endif
#if defined(__WIN__)
    return TRUE;
#endif
}
