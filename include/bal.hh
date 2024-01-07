/*
 * bal.hh
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
#ifndef _BAL_HH_INCLUDED
# define _BAL_HH_INCLUDED

# include "bal.h"
# include <type_traits>
# include <functional>
# include <stdexcept>
# include <cstdlib>
# include <vector>
# include <atomic>
# include <string>
# include <bit>

/** The one and only namespace for libbal. */
namespace bal
{
    struct error
    {
        int code = 0;
        std::string message;

        static error from_last_error()
        {
            bal_error err {};
            auto code = bal_get_error(&err);
            return {code, err.message};
        }
    };

    class exception : public std::runtime_error
    {
    public:
        using std::runtime_error::runtime_error;
        explicit exception(const error& err) : exception(err.message) { }
        ~exception() override = default;
    };

    class address_info
    {
    public:
        address_info() = default;
        explicit address_info(const bal_addrstrings& strings)
        {
            *this = strings;
        }
        virtual ~address_info() = default;

        address_info& operator=(const bal_addrstrings& strings)
        {
            _type = strings.type;
            _host = strings.host;
            _addr = strings.addr;
            _port = strings.port;
            return *this;
        }

        std::string get_type() const { return _type; }
        std::string get_host() const { return _host; }
        std::string get_addr() const { return _addr; }
        std::string get_port() const { return _port; }

        void clear()
        {
            _type.clear();
            _host.clear();
            _addr.clear();
            _port.clear();
        }

    private:
        std::string _type;
        std::string _host;
        std::string _addr;
        std::string _port;
    };

    class address
    {
    public:
        address() = default;
        explicit address(const bal_sockaddr& addr) : _sockaddr(addr) { }
        virtual ~address() = default;

        address& operator=(const bal_sockaddr& addr)
        {
            _sockaddr = addr;
            return *this;
        }

        address_info get_address_info(bool dns_resolve = false) const
        {
            bal_addrstrings strings {};
            if (!bal_get_addrstrings(&_sockaddr, dns_resolve, &strings)) {
                throw exception(error::from_last_error());
            }

            return address_info {strings};
        }

        const bal_sockaddr& get_sockaddr() const
        {
            return _sockaddr;
        }

        void clear()
        {
            std::memset(&_sockaddr, 0, sizeof(bal_sockaddr));
        }

    private:
        bal_sockaddr _sockaddr {};
    };

    class address_list : public std::vector<address>
    {
    public:
        using Base = std::vector<address>;
        using Base::vector;

        explicit address_list(bal_addrlist& addrs)
        {
            *this = addrs;
        }

        address_list& operator=(bal_addrlist& addrs)
        {
            if (!bal_reset_addrlist(&addrs)) {
                throw exception(error::from_last_error());
            }

            auto addr = bal_enum_addrlist(&addrs);
            while (addr != nullptr) {
                Base::emplace_back(address {*addr});
                addr = bal_enum_addrlist(&addrs);
            }

            return *this;
        }
    };

    class policy
    {
    protected:
        policy() = default;
        virtual ~policy() = default;

    public:
        static constexpr bool throw_on_error() noexcept {
            return true;
        }
    };

    class default_policy : public policy
    {
    public:
        default_policy() = default;
        ~default_policy() override = default;
    };

    template<class T>
    concept DerivedFromPolicy = std::derived_from<T, policy>;

    template<DerivedFromPolicy TPolicy, typename T>
    inline T throw_on_policy(const T& value, const T& invalid)
    {
        if (value == invalid) {
            if constexpr(TPolicy::throw_on_error()) {
                throw exception(error::from_last_error());
            }
        }
        return value;
    }

    class initializer
    {
    public:
        initializer()
        {
            if (!bal_isinitialized() && !bal_init()) {
                throw exception(error::from_last_error());
            }
        }

        initializer(const initializer&) = delete;
        initializer(initializer&&) = delete;

        virtual ~initializer()
        {
            [[maybe_unused]] auto cleanup = bal_cleanup();
        }

        initializer& operator=(const initializer&) = delete;
        initializer& operator=(initializer&&) = delete;
    };

    template<bool RAII, DerivedFromPolicy TPolicy>
    class socket_base
    {
    public:
        using async_io_cb = std::function<bool(socket_base*)>;

        socket_base()
        {
            set_default_event_handlers();
        }

        socket_base(const socket_base&) = delete;

        socket_base(socket_base&& other) noexcept
        {
            *this = other;
        }

        socket_base(int addr_fam, int type, int proto) requires RAII : socket_base()
        {
            [[maybe_unused]]
            auto unused = create(addr_fam, type, proto);
        }

        socket_base(int addr_fam, int proto, const std::string& host,
            const std::string& srv) requires RAII : socket_base()
        {
            [[maybe_unused]]
            auto unused = create(addr_fam, proto, host, srv);
        }

        virtual ~socket_base()
        {
            if constexpr(RAII) {
                if (is_valid()) {
                    [[maybe_unused]] auto unused1 = deregister_async_poll();
                    [[maybe_unused]] auto unused2 = bal_close(&_s, true);
                }
            }
        }

        socket_base& operator=(socket_base&) = delete;

        socket_base& operator=(socket_base&& rhs) noexcept
        {
            [[maybe_unused]] const auto* unused = attach(rhs.detach());

            on_read          = rhs.on_read;
            on_write         = rhs.on_write;
            on_connect       = rhs.on_connect;
            on_conn_fail     = rhs.on_conn_fail;
            on_incoming_conn = rhs.on_incoming_conn;
            on_close         = rhs.on_close;
            on_priority      = rhs.on_priority;
            on_error         = rhs.on_error;
            on_invalid       = rhs.on_invalid;
            on_oob_read      = rhs.on_oob_read;
            on_oob_write     = rhs.on_oob_write;

            rhs.set_default_event_handlers();

            return *this;
        }

        bal_socket* get() const noexcept
        {
            return _s;
        }

        bal_descriptor get_descriptor() const noexcept
        {
            return _s != nullptr ? _s->sd : 0;
        }

        bool is_valid() const noexcept
        {
            return _s != nullptr;
        }

        bal_socket* attach(bal_socket* s) noexcept
        {
            bal_socket* tmp = _s;
            _s = s;

            if (tmp != nullptr) {
                tmp->user_data = 0;
            }

            if (_s != nullptr) {
                _s->user_data = to_user_data();
            }

            return tmp;
        }

        bal_socket* detach() noexcept
        {
            const auto ret = attach(nullptr);
            return ret;
        }

        bool create(int addr_fam, int type, int proto)
        {
            [[maybe_unused]] const auto* existing = detach();
            BAL_ASSERT(existing == nullptr);

            const auto ret = bal_create(&_s, to_user_data(), addr_fam, type, proto);
            return throw_on_policy<TPolicy>(ret, false);
        }

        bool create(int addr_fam, int proto, const std::string& host,
            const std::string& srv)
        {
            const auto ret =
                bal_auto_socket(&_s, to_user_data(), addr_fam, proto, host.c_str(),
                    srv.c_str());
            return throw_on_policy<TPolicy>(ret, false);
        }

        bool close(bool destroy = true)
        {
            if (!is_valid()) {
                return false;
            }

            const auto ret = bal_close(&_s, destroy);
            return throw_on_policy<TPolicy>(ret, false);
        }

        bool shutdown(int how)
        {
            const auto ret = bal_shutdown(_s, how);
            return throw_on_policy<TPolicy>(ret, false);
        }

        bool async_poll(uint32_t mask = BAL_EVT_NORMAL)
        {
            if (!is_valid()) {
                return false;
            }

            const auto ret = bal_async_poll(_s, &socket_base::_on_async_io, mask);
            return throw_on_policy<TPolicy>(ret, false);
        }

        bool deregister_async_poll() noexcept
        {
            return is_valid() ? bal_async_poll(_s, nullptr, 0U) : false;
        }

        bool connect(const std::string& host, const std::string& port)
        {
            const auto ret = bal_connect(_s, host.c_str(), port.c_str());
            return throw_on_policy<TPolicy>(ret, false);
        }

        ssize_t send(const void* data, bal_iolen len, int flags = MSG_NOSIGNAL) const
        {
            const auto ret = bal_send(_s, data, len, flags);
            return throw_on_policy<TPolicy>(ret, -1L);
        }

        ssize_t sendto(const std::string& host, const std::string& port,
            const void* data, bal_iolen len, int flags = MSG_NOSIGNAL) const
        {
            const auto ret =
                bal_sendto(_s, host.c_str(), port.c_str(), data, len, flags);
            return throw_on_policy<TPolicy>(ret, -1L);
        }

        ssize_t recv(void* data, bal_iolen len, int flags) const
        {
            const auto ret = bal_recv(_s, data, len, flags);
            return throw_on_policy<TPolicy>(ret, -1L);
        }

        ssize_t recvfrom(void* data, bal_iolen len, int flags, address& whence) const
        {
            whence.clear();

            bal_sockaddr tmp {};
            const auto ret = bal_recvfrom(_s, data, len, flags, &tmp);
            if (ret) {
                whence = tmp;
            }

            return throw_on_policy<TPolicy>(ret, -1L);
        }

        bool bind(const std::string& addr, const std::string& srv) const
        {
            const auto ret = bal_bind(_s, addr.c_str(), srv.c_str());
            return throw_on_policy<TPolicy>(ret, false);
        }

        bool bind_all(const std::string& srv) const
        {
            const auto ret = bal_bindall(_s, srv.c_str());
            return throw_on_policy<TPolicy>(ret, false);
        }

        bool listen(int backlog = SOMAXCONN)
        {
            const auto ret = bal_listen(_s, backlog);
            return throw_on_policy<TPolicy>(ret, false);
        }

        bool accept(socket_base& client_sock, address& client_addr) const
        {
            [[maybe_unused]] const auto* existing = client_sock.detach();
            BAL_ASSERT(existing == nullptr);

            client_addr.clear();

            bal_socket* s = nullptr;
            bal_sockaddr addr {};
            const auto ret = bal_accept(_s, &s, &addr);
            if (ret) {
                [[maybe_unused]] const auto* unused = client_sock.attach(s);
                client_addr = addr;
            }

            return throw_on_policy<TPolicy>(ret, false);
        }

        bool get_option(int level, int name, void* optval, socklen_t len) const
        {
            const auto ret = bal_get_option(_s, level, name, optval, len);
            return throw_on_policy<TPolicy>(ret, false);
        }

        bool set_option(int level, int name, const void* optval, socklen_t len) const
        {
            const auto ret = bal_set_option(_s, level, name, optval, len);
            return throw_on_policy<TPolicy>(ret, false);
        }

        bool get_broadcast(int* value) const
        {
            const auto ret = bal_get_broadcast(_s, value);
            return throw_on_policy<TPolicy>(ret, false);
        }

        bool set_broadcast(int value) const
        {
            const auto ret = bal_set_broadcast(_s, value);
            return throw_on_policy<TPolicy>(ret, false);
        }

        bool get_debug(int* value) const
        {
            const auto ret = bal_get_debug(_s, value);
            return throw_on_policy<TPolicy>(ret, false);
        }

        bool set_debug(int value) const
        {
            const auto ret = bal_set_debug(_s, value);
            return throw_on_policy<TPolicy>(ret, false);
        }

        bool get_linger(bal_linger* seconds) const
        {
            const auto ret = bal_get_linger(_s, seconds);
            return throw_on_policy<TPolicy>(ret, false);
        }

        bool set_linger(bal_linger seconds) const
        {
            const auto ret = bal_set_linger(_s, seconds);
            return throw_on_policy<TPolicy>(ret, false);
        }

        bool get_keepalive(int* value) const
        {
            const auto ret = bal_get_keepalive(_s, value);
            return throw_on_policy<TPolicy>(ret, false);
        }

        bool set_keepalive(int value) const
        {
            const auto ret = bal_set_keepalive(_s, value);
            return throw_on_policy<TPolicy>(ret, false);
        }

        bool get_oobinline(int* value) const
        {
            const auto ret = bal_get_oobinline(_s, value);
            return throw_on_policy<TPolicy>(ret, false);
        }

        bool set_oobinline(int value) const
        {
            const auto ret = bal_set_oobinline(_s, value);
            return throw_on_policy<TPolicy>(ret, false);
        }

        bool get_reuseaddr(int* value) const
        {
            const auto ret = bal_get_reuseaddr(_s, value);
            return throw_on_policy<TPolicy>(ret, false);
        }

        bool set_reuseaddr(int value) const
        {
            const auto ret = bal_set_reuseaddr(_s, value);
            return throw_on_policy<TPolicy>(ret, false);
        }

        bool get_sendbuf_size(int* value) const
        {
            const auto ret = bal_get_sendbuf_size(_s, value);
            return throw_on_policy<TPolicy>(ret, false);
        }

        bool set_sendbuf_size(int value) const
        {
            const auto ret = bal_set_sendbuf_size(_s, value);
            return throw_on_policy<TPolicy>(ret, false);
        }

        bool get_recvbuf_size(int* value) const
        {
            const auto ret = bal_get_recvbuf_size(_s, value);
            return throw_on_policy<TPolicy>(ret, false);
        }

        bool set_recvbuf_size(int value) const
        {
            const auto ret = bal_set_recvbuf_size(_s, value);
            return throw_on_policy<TPolicy>(ret, false);
        }

        bool get_send_timeout(bal_tvsec* sec, bal_tvusec* usec) const
        {
            const auto ret = bal_get_send_timeout(_s, sec, usec);
            return throw_on_policy<TPolicy>(ret, false);
        }

        bool set_send_timeout(bal_tvsec sec, bal_tvusec usec) const
        {
            const auto ret = bal_set_send_timeout(_s, sec, usec);
            return throw_on_policy<TPolicy>(ret, false);
        }

        bool get_recv_timeout(bal_tvsec* sec, bal_tvusec* usec) const
        {
            const auto ret = bal_get_recv_timeout(_s, sec, usec);
            return throw_on_policy<TPolicy>(ret, false);
        }

        bool set_recv_timeout(bal_tvsec sec, bal_tvusec usec) const
        {
            const auto ret = bal_set_recv_timeout(_s, sec, usec);
            return throw_on_policy<TPolicy>(ret, false);
        }

        bool set_io_mode(bool async) const
        {
            const auto ret = bal_set_io_mode(_s, async);
            return throw_on_policy<TPolicy>(ret, false);
        }

        size_t get_recvqueue_size() const
        {
            const auto ret = bal_get_recvqueue_size(_s);
            return throw_on_policy<TPolicy>(ret, 0);
        }

        error get_error(bool extended) const
        {
            bal_error err {};
            if (extended) {
                err.code = bal_get_error_ext(&err);
            } else {
                err.code = bal_get_error(&err);
            }

            return {err.code, err.message};
        }

        bool is_readable() const noexcept
        {
            return bal_is_readable(_s);
        }

        bool is_writable() const noexcept
        {
            return bal_is_writable(_s);
        }

        bool is_listening() const noexcept
        {
            return bal_is_listening(_s);
        }

        static bool resolve_host(const std::string& host, address_list& addrs)
        {
            addrs.clear();

            bal_addrlist addrlist {};
            auto ret = bal_resolve_host(host.c_str(), &addrlist);
            if (ret) {
                addrs = addrlist;
                ret = bal_free_addrlist(&addrlist);
            }

            return throw_on_policy<TPolicy>(ret, false);
        }

        bool get_peer_addr(address& peer_addr)
        {
            peer_addr.clear();

            bal_sockaddr addr {};
            const auto ret = bal_get_peer_addr(_s, &addr);
            if (ret) {
                peer_addr = addr;
            }

            return throw_on_policy<TPolicy>(ret, false);
        }

        void want_write_events(bool want)
        {
            if (want) {
                bal_addtomask(_s, BAL_EVT_WRITE);
            } else {
                bal_remfrommask(_s, BAL_EVT_WRITE);
            }
        }

        static socket_base* from_user_data(bal_socket* s)
        {
            return std::bit_cast<socket_base*>(s->user_data);
        }

        uintptr_t to_user_data() const
        {
            return std::bit_cast<uintptr_t>(this);
        }

        async_io_cb on_read;
        async_io_cb on_write;
        async_io_cb on_connect;
        async_io_cb on_conn_fail;
        async_io_cb on_incoming_conn;
        async_io_cb on_close;
        async_io_cb on_priority;
        async_io_cb on_error;
        async_io_cb on_invalid;
        async_io_cb on_oob_read;
        async_io_cb on_oob_write;

        void set_default_event_handlers()
        {
            on_read = nullptr;
            on_write = nullptr;
            on_connect = nullptr;
            on_conn_fail = nullptr;
            on_incoming_conn = nullptr;
            on_close = [](socket_base* sock)
            {
                [[maybe_unused]] const auto closed = sock->close();
                return false;
            };
            on_priority = nullptr;
            on_error = [](socket_base* sock)
            {
                [[maybe_unused]] const auto closed = sock->close();
                return false;
            };
            on_invalid = nullptr;
            on_oob_read = nullptr;
            on_oob_write = nullptr;
        }

    protected:
        static void _on_async_io(bal_socket* s, uint32_t events)
        {
            try {
                socket_base* self = from_user_data(s);
                BAL_ASSERT(self != nullptr);

                if (self == nullptr) {
                    _bal_dbglog("no user_data for socket " BAL_SOCKET_SPEC " (0x%"
                        PRIxPTR ", mask = %08" PRIx32 ")", s->sd,
                        std::bit_cast<uintptr_t>(s), s->state.mask);
                    return;
                }

                auto print_early_return = [s, self](uint32_t evt)
                {
# if defined(BAL_DBGLOG)
                    _bal_dbglog("early return for socket " BAL_SOCKET_SPEC " (0x%"
                        PRIxPTR ", evt = %08" PRIx32 ", self = 0x%" PRIxPTR ")",
                        s->sd, std::bit_cast<uintptr_t>(s), evt,
                        std::bit_cast<uintptr_t>(self));
# else
                    BAL_UNUSED(s);
                    BAL_UNUSED(self);
                    BAL_UNUSED(evt);
# endif
                };

                if (bal_isbitset(events, BAL_EVT_READ) && self->on_read &&
                    !self->on_read(self)) {
                    print_early_return(BAL_EVT_READ);
                    return;
                }

                if (bal_isbitset(events, BAL_EVT_WRITE) && self->on_write &&
                    !self->on_write(self)) {
                    print_early_return(BAL_EVT_WRITE);
                    return;
                }

                if (bal_isbitset(events, BAL_EVT_CONNECT) && self->on_connect &&
                    !self->on_connect(self)) {
                    print_early_return(BAL_EVT_CONNECT);
                    return;
                }

                if (bal_isbitset(events, BAL_EVT_CONNFAIL) && self->on_conn_fail &&
                    !self->on_conn_fail(self)) {
                    print_early_return(BAL_EVT_CONNFAIL);
                    return;
                }

                if (bal_isbitset(events, BAL_EVT_ACCEPT) && self->on_incoming_conn &&
                    !self->on_incoming_conn(self)) {
                    print_early_return(BAL_EVT_ACCEPT);
                    return;
                }

                if (bal_isbitset(events, BAL_EVT_CLOSE) && self->on_close &&
                    !self->on_close(self)) {
                    print_early_return(BAL_EVT_CLOSE);
                    return;
                }

                if (bal_isbitset(events, BAL_EVT_PRIORITY) && self->on_priority &&
                    !self->on_priority(self)) {
                    print_early_return(BAL_EVT_PRIORITY);
                    return;
                }

                if (bal_isbitset(events, BAL_EVT_ERROR) && self->on_error &&
                    !self->on_error(self)) {
                    print_early_return(BAL_EVT_ERROR);
                    return;
                }

                if (bal_isbitset(events, BAL_EVT_INVALID) && self->on_invalid &&
                    !self->on_invalid(self)) {
                    print_early_return(BAL_EVT_INVALID);
                    return;
                }

                if (bal_isbitset(events, BAL_EVT_OOBREAD) && self->on_oob_read &&
                    !self->on_oob_read(self)) {
                    print_early_return(BAL_EVT_OOBREAD);
                    return;
                }

                if (bal_isbitset(events, BAL_EVT_OOBWRITE) && self->on_oob_write &&
                    !self->on_oob_write(self)) {
                    print_early_return(BAL_EVT_OOBWRITE);
                    return;
                }
            } catch (bal::exception& ex) {
                _bal_dbglog("error: caught exception: '%s'!", ex.what());
            }
        }

    private:
        bal_socket* _s = nullptr;
    };

    using scoped_socket = socket_base<true, default_policy>;
    using manual_socket = socket_base<false, default_policy>;

} // !namespace bal

#endif // !_BAL_HH_INCLUDED
