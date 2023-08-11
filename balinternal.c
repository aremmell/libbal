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
    WORD wVer  = MAKEWORD(WSOCK_MINVER, WSOCK_MAJVER);
    WSADATA wd = {0};

    if (0 != WSAStartup(wVer, &wd)) {
        _bal_handlewin32err(WSAGetLastError());
        return false;
    }
#endif

    bool init = _bal_once(&_bal_static_once_init, &_bal_static_once_init_func);
    assert(init);

    if (!init) {
        _bal_selflog("error: _bal_static_once_init_func failed");
        return false;
    }

    init = _bal_initasyncselect();
    assert(init);

    if (!init) {
        _bal_selflog("error: _bal_initasyncselect failed");
        bool cleanup = _bal_cleanupasyncselect();
        assert(cleanup);
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
        _bal_selflog("error: _bal_cleanupasyncselect failed");
        return false;
    }

    return true;
}

int _bal_asyncselect(const bal_socket* s, bal_async_callback proc, uint32_t mask)
{
    /* it is possible that the call to this function is in response to a call
     * made during the iteration of lst_active. we cannot add
     * or remove entries here; instead, add them to separate lists that are
     * processed after any potential iteration has finished. */

    if (!_bal_get_boolean(&_bal_asyncselect_init)) {
        assert(!"async I/O not initialized");
        return BAL_FALSE;
    }

    int r = BAL_FALSE;
    assert(s && proc);

    if (s && proc) {
        if (_bal_mutex_lock(&_bal_as_container.mutex)) {
            if (0u == mask) {
                bal_selectdata* d = NULL;
                bool found = _bal_list_find(_bal_as_container.lst, s->sd, &d);
                if (found) {
                    bool added = _bal_defer_remove_socket(d);
                    if (added) {
                        _bal_selflog("added socket "BAL_SOCKET_SPEC" to defer "
                                     " remove list", s->sd);
                    } else {
                        assert(!"failed to add socket to defer remove list");
                    }
                } else {
                    _bal_selflog("failed to find socket "BAL_SOCKET_SPEC" in"
                                 " list; ignoring", s->sd);
                }
            } else {
                bal_selectdata* d = NULL;
                if (_bal_list_find(_bal_as_container.lst, s->sd, &d)) {
                    assert(NULL != d);
                    if (d) {
                        d->mask = mask;
                        d->proc = proc;
                        r       = BAL_TRUE;
                        _bal_selflog("updated socket "BAL_SOCKET_SPEC, s->sd);
                    }
                } else {
                    bal_selectdata* d = calloc(1, sizeof(bal_selectdata));
                    assert(NULL != d);

                    if (d) {
                        bool success = false;
                        if (BAL_FALSE != bal_setiomode(s, true)) {
                            d->mask = mask;
                            d->proc = proc;
                            d->s    = (bal_socket*)s;

                            success = _bal_defer_add_socket(d);
                            r = success ? BAL_TRUE : BAL_FALSE;
                        }

                        if (success) {
                            _bal_selflog("added socket "BAL_SOCKET_SPEC" to"
                                         " defer add list", s->sd);
                        } else {
                            bal_safefree(&d);
                            assert(!"failed to add socket to defer add list");
                        }
                    }
                }
            }

            bool unlocked = _bal_mutex_unlock(&_bal_as_container.mutex);
            BAL_ASSERT_UNUSED(unlocked, unlocked);
        }
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
        assert(create);

        if (!create) {
            (void)_bal_handleerr(errno);
            _bal_selflog("error: failed to create list(s)");
            return false;
        }
    }

    bal_mutex* mutexes[] = {
        &_bal_as_container.mutex,
        &_bal_as_container.s_mutex
    };

    for (size_t n = 0; n < bal_countof(mutexes); n++) {
        create = _bal_mutex_create(mutexes[n]);
        assert(create);

        if (!create) {
            _bal_selflog("error: failed to create mutex(es)");
            return false;
        }
    }

    bal_condition* conditions[] = {
        &_bal_as_container.s_cond
    };

    for (size_t n = 0; n < bal_countof(conditions); n++) {
        create = _bal_cond_create(conditions[n]);
        assert(create);

        if (!create) {
            _bal_selflog("error: failed to create condition variable(s)");
            return false;
        }
    }

    struct
    {
        bal_thread* thread;
        bal_thread_func func;
    } threads[] = {
        { &_bal_as_container.thread, &_bal_eventthread },
        { &_bal_as_container.s_thread, &_bal_syncthread }
    };

    for (size_t n = 0; n < bal_countof(threads); n++) {
#if defined(__WIN__)
        *threads[n].thread = _beginthreadex(NULL, 0u, threads[n].func,
            &_bal_as_container, 0u, NULL);
        assert(0ull != *threads[n].thread);

        if (0ull == *threads[n].thread) {
            (void)_bal_handleerr(errno);
            _bal_selflog("error: failed to create thread(s)");
            return false;
        }
#else
        int op = pthread_create(threads[n].thread, NULL, threads[n].func,
            &_bal_as_container);
        assert(0 == op);

        if (0 != op) {
            (void)_bal_handleerr(op);
            _bal_selflog("error: failed to create thread(s)");
            return false;
        }
#endif
    }

    _bal_set_boolean(&_bal_asyncselect_init, true);
    _bal_selflog("async I/O initialized");

    return true;
}

bool _bal_cleanupasyncselect(void)
{
    _bal_set_boolean(&_bal_as_container.die, true);

    bal_thread* threads[] = {
        &_bal_as_container.thread,
        &_bal_as_container.s_thread
    };

    for (size_t n = 0; n < bal_countof(threads); n++) {
        _bal_selflog("joining thread %zu/%zu...", n + 1, bal_countof(threads));
#if defined(__WIN__)
        (void)WaitForSingleObject((HANDLE)_bal_as_container.thread, INFINITE);
#else
        (void)pthread_join(_bal_as_container.thread, NULL);
#endif
        _bal_selflog("joined thread %zu/%zu successfully", n + 1, bal_countof(threads));
    }

    bool cleanup = true;
    bal_list* lists[] = {
        _bal_as_container.lst,
        _bal_as_container.lst_add,
        _bal_as_container.lst_rem
    };

    for (size_t n = 0; n < bal_countof(lists); n++) {
        bool destroy = _bal_list_destroy(&lists[n]);
        assert(destroy);
        cleanup &= destroy;
    }

    bal_mutex* mutexes[] = {
        &_bal_as_container.mutex,
        &_bal_as_container.s_mutex
    };

    for (size_t n = 0; n < bal_countof(mutexes); n++) {
        bool destroy = _bal_mutex_destroy(mutexes[n]);
        assert(destroy);
        cleanup &= destroy;
    }

    bal_condition* conditions[] = {
        &_bal_as_container.s_cond
    };

    for (size_t n = 0; n < bal_countof(conditions); n++) {
        bool destroy = _bal_cond_destroy(conditions[n]);
        assert(destroy);
        cleanup &= destroy;
    }

    _bal_set_boolean(&_bal_asyncselect_init, false);
    _bal_selflog("async I/O cleaned up %s", cleanup ?
        "successfully" : "with errors");

    return cleanup;
}

bool _bal_defer_add_socket(bal_selectdata* d)
{
    bool retval = false;

    if (d) {
        bool locked = _bal_mutex_lock(&_bal_as_container.s_mutex);
        assert(locked);

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
        assert(locked);

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

            if (!r)
                res->_p = res->_ai;
            else
                res->_p = res->_ai = NULL;
        }
    }

    if (BAL_TRUE != r && BAL_FALSE != r) {
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

int _bal_getlasterror(const bal_socket* s, bal_error* err)
{
    int r = BAL_FALSE;

    if (err) {
        memset(err, 0, sizeof(bal_error));

        if (s) {
            err->code = bal_geterror(s);
        } else {
#if defined(__WIN__)
            err->code = WSAGetLastError();
#else
            err->code = errno;
#endif
        }

#if defined(__WIN__)
        if (0 != FormatMessageA(0x00001200u, NULL, err->code, 0u, err->desc,
            BAL_MAXERROR, NULL))
            r = BAL_TRUE;
#else
        if (err->code == EAI_AGAIN  || err->code == EAI_BADFLAGS ||
            err->code == EAI_FAIL   || err->code == EAI_FAMILY   ||
            err->code == EAI_MEMORY || err->code == EAI_NONAME   ||
            err->code == EAI_NODATA || err->code == EAI_SERVICE  ||
            err->code == EAI_SOCKTYPE) {
            if (0 == _bal_retstr(err->desc, gai_strerror(err->code), BAL_MAXERROR))
                r = BAL_TRUE;
        } else {
            if (0 == _bal_retstr(err->desc, (const char*)strerror(err->code),
                BAL_MAXERROR))
                r = BAL_TRUE;
        }
#endif
    }

    return r;
}

bool __bal_setlasterror(int err, const char* func, const char* file, int line)
{
#if defined(__WIN__)
    WSASetLastError(err);
#else
    errno = err;
#endif

return false;
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
    assert(NULL != asc);

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

                bool iterate = _bal_list_iterate(asc->lst, &epd,
                    &__bal_list_event_prepare);
                assert(iterate);

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
    assert(NULL != asc);

    while (!_bal_get_boolean(&asc->die)) {
        if (_bal_mutex_lock(&asc->s_mutex)) {
            while (_bal_list_empty(asc->lst_add) && _bal_list_empty(asc->lst_rem) &&
                  !_bal_get_boolean(&asc->die)) {
#if !defined(__WIN__)
                /* seconds; absolute fixed time. */
                bal_wait wait = {time(NULL) + 2, 0};
#else
                /* msec; relative from now. */
                bal_wait wait = 2000;
#endif
                (void)_bal_condwait_timeout(
                    &asc->s_cond,
                    &asc->s_mutex,
                    &wait
                );
            }

            if (_bal_get_boolean(&asc->die)) {
                bool unlocked = _bal_mutex_unlock(&asc->s_mutex);
                BAL_ASSERT_UNUSED(unlocked, unlocked);
                break;
            }

            if (_bal_mutex_trylock(&asc->mutex)) {
                if (!_bal_list_empty(asc->lst_add)) {
                    _bal_selflog("processing deferred socket adds...");

                    bool add = _bal_list_iterate(asc->lst_add, asc->lst,
                        &__bal_list_add_entries);
                    BAL_ASSERT_UNUSED(add, add);

                    add = _bal_list_remove_all(asc->lst_add);
                    BAL_ASSERT_UNUSED(add, add);
                }

                if (!_bal_list_empty(asc->lst_rem)) {
                    _bal_selflog("processing deferred socket removals...");

                    bool rem = _bal_list_iterate(asc->lst_rem, asc->lst,
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
    assert(set && asc);
    if (set && asc) {
        _bal_list_dispatch_data ldd = {
            set,
            type
        };

        bool iterate = _bal_list_iterate(asc->lst, &ldd, &__bal_list_dispatch_events);
        assert(iterate);
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
        ok = _bal_list_iterate(lst, &lfd, &__bal_list_find_key) && lfd.found;
    }

    return ok;
}

bool _bal_list_empty(bal_list* lst)
{
    return !lst || !lst->head;
}

bool _bal_list_iterate(bal_list* lst, void* ctx, bal_list_iter_callback cb)
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
                *val = node->val;
                ok = _bal_list_destroy_node(&node);
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
        bal_safefree(lst);
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

        bal_safefree(node);
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
                assert(!"invalid event type");
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
                _bal_selflog("added socket "BAL_SOCKET_SPEC" to defer"
                             " remove list", key);
            } else {
                assert(!"failed to add socket to defer remove list");
            }
        }
    }

    return true;
}

bool __bal_list_remove_entries(bal_descriptor key, bal_selectdata* val, void* ctx)
{
    bal_list* lst = (bal_list*)ctx;

    int ret = bal_close(val->s);
    BAL_ASSERT_UNUSED(ret, BAL_TRUE == ret);

    bal_selectdata* d = NULL;
    bool removed = _bal_list_remove(lst, key, &d);
    BAL_ASSERT_UNUSED(removed, removed);
    bal_safefree(&d);

    if (removed)
        _bal_selflog("closed and removed socket " BAL_SOCKET_SPEC " from list", key);

    return true;
}

bool __bal_list_add_entries(bal_descriptor key, bal_selectdata* val, void* ctx)
{
    bal_list* lst = (bal_list*)ctx;
    bool added    = _bal_list_add(lst, key, val);
    BAL_ASSERT_UNUSED(added, added);

    if (added)
        _bal_selflog("added socket " BAL_SOCKET_SPEC " to list", key);

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
        return 0 == op ? true : _bal_handleerr(op);
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
            ? true : _bal_handlewin32err(GetLastError());

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

#if defined(BAL_SELFLOG)
void __bal_selflog(const char* func, const char* file, uint32_t line,
    const char* format, ...)
{
    va_list args;
    va_list args2;
    va_start(args, format);
    va_copy(args2, args);

    int prnt_len = vsnprintf(NULL, 0, format, args);

    va_end(args);
    assert(prnt_len > 0);

    char* buf = calloc(prnt_len + 1, sizeof(char));
    assert(NULL != buf);

    if (buf) {
        char prefix[256] = {0};
        int pfx_len = snprintf(prefix, 256, "%s (%s:%"PRIu32"): ", func, file, line);
        assert(pfx_len > 0 && pfx_len < 256);

        va_start(args2, format);
        (void)vsnprintf(buf, prnt_len + 1, format, args2);
        va_end(args2);

        printf("%s%s\n", prefix, buf);

        bal_safefree(&buf);
    }
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
