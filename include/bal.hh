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

        explicit address_list(bal_addrlist* addrs)
        {
            if (!bal_reset_addrlist(addrs)) {
                throw exception(error::from_last_error());
            }

            auto addr = bal_enum_addrlist(addrs);
            while (addr != nullptr) {
                Base::emplace_back(address {*addr});
                addr = bal_enum_addrlist(addrs);
            }
        }
    };

    class initializer
    {
    public:
        initializer()
        {
            if (!bal_init()) {
                throw exception(error::from_last_error());
            }
        }

        virtual ~initializer()
        {
            [[maybe_unused]] auto cleanup = bal_cleanup();
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

    enum class shutdown_mode : int
    {
        read = BAL_SHUT_RD,
        write = BAL_SHUT_WR,
        readwrite = BAL_SHUT_RDWR
    };

    template<bool RAII, DerivedFromPolicy TPolicy>
    class socket_base
    {
    public:
        socket_base(int addr_fam, int type, int proto) requires RAII
        {
            const auto created = bal_create(&_s, addr_fam, type, proto);
            [[maybe_unused]]
            const auto excpt = throw_on_policy<TPolicy>(created, false);
        }

        socket_base(int addr_fam, int proto, const std::string& host,
            const std::string& srv) requires RAII
        {
            const auto created =
                bal_auto_socket(&_s, addr_fam, proto, host.c_str(), srv.c_str());
            [[maybe_unused]]
            const auto excpt = throw_on_policy<TPolicy>(created, false);
        }

        socket_base(socket_base&) = delete;

        explicit socket_base(bal_socket* s) : _s(s) { }

        virtual ~socket_base()
        {
            if constexpr(RAII) {
                if (is_valid()) {
                    [[maybe_unused]] auto unused = bal_close(&_s, true);
                }
            }
        }

        socket_base& operator=(socket_base&) = delete;

        socket_base& operator=(socket_base&& rhs)
        {
            [[maybe_unused]] auto unused = attach(rhs.detach());
            return *this;
        }

        bal_socket* get() const noexcept
        {
            return _s;
        }

        bool is_valid() const noexcept
        {
            return _s != nullptr;
        }

        bal_socket* attach(bal_socket* s) noexcept
        {
            bal_socket* tmp = _s;
            _s = s;
            return tmp;
        }

        bal_socket* detach() noexcept
        {
            return attach(nullptr);
        }

        bool close(bool destroy = true)
        {
            if (!is_valid()) {
                return false;
            }

            const auto closed = bal_close(&_s, destroy);
            return throw_on_policy<TPolicy>(closed, false);
        }

        bool shutdown(shutdown_mode how)
        {
            const auto sd = bal_shutdown(_s, how);
            return throw_on_policy<TPolicy>(sd, false);
        }

        bool connect(const std::string& host, const std::string& port)
        {
            const auto conn = bal_connect(_s, host.c_str(), port.c_str());
            return throw_on_policy<TPolicy>(conn, false);
        }

        ssize_t send(const void* data, bal_iolen len, int flags)
        {
            const auto sent = bal_send(_s, data, len, flags);
            return throw_on_policy<TPolicy>(sent, -1);
        }

        ssize_t sendto(const std::string& host, const std::string& port,
            const void* data, bal_iolen len, int flags)
        {
            const auto sent =
                bal_sendto(_s, host.c_str(), port.c_str(), data, len, flags);
            return throw_on_policy<TPolicy>(sent, -1);
        }

        ssize_t recv(void* data, bal_iolen len, int flags)
        {
            const auto read = bal_recv(_s, data, len, flags);
            return throw_on_policy<TPolicy>(read, -1);
        }

        ssize_t recvfrom(void* data, bal_iolen len, int flags, address& whence)
        {
            whence.clear();

            bal_sockaddr tmp {};
            const auto read = bal_recvfrom(_s, data, len, flags, &tmp);
            if (read) {
                whence = tmp;
            }

            return throw_on_policy<TPolicy>(read, -1);
        }

        bool bind(const std::string& addr, const std::string& srv)
        {
            const auto bound = bal_bind(_s, addr.c_str(), srv.c_str());
            return throw_on_policy<TPolicy(bound, false);
        }

        bool bind_all(const std::string& srv)
        {
            const auto bound = bal_bindall(_s, srv.c_str());
            return throw_on_policy<TPolicy(bound, false);
        }

        bool listen(int backlog = SOMAXCONN)
        {
            const auto listening = bal_listen(_s, backlog);
            return throw_on_policy<TPolicy(listening, false);
        }

        bool accept(socket_base& client_sock, address& client_addr)
        {
            const auto existing = client_sock.detach();
            BAL_ASSERT(existing == nullptr);

            client_addr.clear();

            bal_socket* s = nullptr;
            bal_sockaddr addr {};
            const auto accepted = bal_accept(_s, &s, &addr);
            if (accepted) {
                [[maybe_unused]] auto unused = client_sock.attach(s);
                client_addr = addr;
            }

            return throw_on_policy<TPolicy>(accepted, false);
        }

    private:
        bal_socket* _s = nullptr;
    };

    using scoped_socket = socket_base<true, default_policy>;
    using manual_socket = socket_base<false, default_policy>;

} // !namespace bal

#endif // !_BAL_HH_INCLUDED
