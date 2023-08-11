#include "balinternal.h"


/**************************** Internal Functions ******************************/


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

#if defined(BAL_SELFLOG)
    bal_error lasterr = {0};
    int get_err = _bal_getlasterror(NULL, &lasterr);

    if (BAL_TRUE != get_err) {
#if defined(__WIN__)
        lasterr.code = WSAGetLastError();
#else
        lasterr.code = errno;
#endif
        strncpy(lasterr.desc, BAL_AS_UNKNWN, strnlen(BAL_AS_UNKNWN, BAL_MAXERROR));
    }

    __bal_selflog(func, file, line, "error: %d (%s)", lasterr.code, lasterr.desc);
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

BALTHREAD _bal_eventthread(void* p)
{
    bal_asyncselect_data* asd = (bal_asyncselect_data*)p;
    assert(NULL != asd);

    while (!_bal_get_boolean(&asd->die)) {
        if (_bal_mutex_lock(&asd->mutex)) {
            if (!_bal_list_empty(asd->lst)) {
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

                bool iterate = _bal_list_iterate(asd->lst, &epd,
                    &__bal_list_event_prepare);
                assert(iterate);

                struct timeval tv = {0, 0};
                int poll_res = select(epd.high_watermark + 1, &fd_read, &fd_write,
                    &fd_except, &tv);

                if (-1 != poll_res) {
                    _bal_dispatchevents(&fd_read, asd, BAL_S_READ);
                    _bal_dispatchevents(&fd_write, asd, BAL_S_WRITE);
                    _bal_dispatchevents(&fd_except, asd, BAL_S_EXCEPT);
                }
            }

            bool unlocked = _bal_mutex_unlock(&asd->mutex);
            BAL_ASSERT_UNUSED(unlocked, unlocked);
        }

        _bal_yield_thread();
    }

#if defined(__WIN__)
    return 0u;
#else
    return NULL;
#endif
}

bool _bal_initasyncselect(bal_asyncselect_data* asd)
{
    bool valid = _bal_validptr(asd);

    if (valid) {
#if defined(__HAVE_STDATOMICS__)
        atomic_init(&asd->die, false);
#else
        asd->die = false;
#endif
        bool create = _bal_list_create(&asd->lst);
        assert(create);

        create = _bal_mutex_init(&asd->m);
        assert(create);

        create = _bal_cond_create(&asd->cond);
        assert(create);

#if defined(__WIN__)
        asd->t = _beginthreadex(NULL, 0u, _bal_eventthread, asd, 0u, NULL);
        assert(0ull != asd->t);

        if (0ull == asd->t)
            valid = _bal_handleerr(errno);
#else
        int op = pthread_create(&asd->t, NULL, _bal_eventthread, asd);
        assert(0 == op);

        if (0 != op)
            valid = _bal_handleerr(op);
#endif
    }

    return valid;
}

bool _bal_cleanupasyncselect(bal_asyncselect_data* asd)
{
    bool valid = _bal_validptr(asd);

    if (valid) {
        _bal_set_boolean(&asd->die, true);

        _bal_selflog("joining event thread...");
#if defined(__WIN__)
        (void)WaitForSingleObject((HANDLE)asd->t, INFINITE);
#else
        (void)pthread_join(&asd->t, NULL);
#endif
        _bal_selflog("event thread joined\n");

        bool destroy = _bal_list_destroy(&asd->lst);
        assert(destroy);

        valid &= destroy;

        destroy = _bal_mutex_destroy(&asd->mutex);
        assert(destroy);

        valid &= destroy;

        destroy = _bal_cond_destroy(&asd->cond);
        assert(destroy);

        valid &= destroy;

/*         if (_bal_list_remove_all(&lst_active)) {

            return r = BAL_TRUE;
        } */

        _bal_set_boolean(&_bal_asyncselect_init, false);
        _bal_selflog("async select handler cleaned up");
    }

    return valid;
}

void _bal_dispatchevents(fd_set* set, bal_asyncselect_data* asd, uint32_t type)
{
    assert(set && asd);
    if (set && asd) {
        bal_list* lst_remove = NULL;
        bool created         = _bal_list_create(&lst_remove);
        assert(created);

        if (!created)
            return;

        _bal_list_dispatch_data ldd = {
            set,
            lst_remove,
            type
        };

        bool iterate = _bal_list_iterate(asd->lst, &ldd, &__bal_list_dispatch_events);
        assert(iterate);

        if (iterate) {
            /* remove closed sockets. */
            if (!_bal_list_empty(lst_remove)) {
                iterate = _bal_list_iterate(lst_remove, asd->lst,
                    &__bal_list_remove_entries);
                assert(iterate);
            }
        }

        _bal_list_destroy(&lst_remove);
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

bool _list_create_node(bal_list_node** node, bal_descriptor key, bal_selectdata* val)
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

bool _list_destroy(bal_list** lst)
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

        if (BAL_E_CLOSE == event) {
            /* since we are actively iterating the linked list, we cannot remove
             * an entry here. instead, add the socket to a separate list that
             * is processed after this list is iterated to the end. */
            bool added = _bal_list_add(ldd->lst, key, val);
            BAL_ASSERT_UNUSED(added, added);
        }
    }

    return true;
}

bool __bal_list_remove_entries(bal_descriptor key, bal_selectdata* val, void* ctx)
{
    bal_list* lst = (bal_list*)ctx;
    bal_descriptor sd = val->s->sd;

    int ret = bal_close(val->s);
    BAL_ASSERT_UNUSED(ret, BAL_TRUE == ret);

    bal_selectdata* d = NULL;
    ret = _bal_list_remove(lst, sd, &d);
    BAL_ASSERT_UNUSED(ret, BAL_TRUE == ret);
    bal_safefree(&d);

    _bal_selflog("closed and removed socket " BAL_SOCKET_SPEC " from list", sd);
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

void _bal_yield_thread(void)
{
#if defined(__WIN__)
    Sleep(1);
#else
    int yield = sched_yield();
    assert(0 == yield);
#endif
}

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
BOOL CALLBACK _bal_static_once_init(PINIT_ONCE ponce, PVOID param, PVOID* ctx)
{
    BAL_UNUSED(ponce);
    BAL_UNUSED(param);
    BAL_UNUSED(ctx);
# if defined(__HAVE_STDATOMICS__)
    atomic_init(&_bal_asyncselect_init, false);
# else
    _bal_asyncselect_init = false;
# endif
    return TRUE;
}
#else
void _bal_static_once_init(void)
{
# if defined(__HAVE_STDATOMICS__)
    atomic_init(&_bal_asyncselect_init, false);
# else
    _bal_asyncselect_init = false;
# endif
}
#endif
