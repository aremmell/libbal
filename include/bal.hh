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
# include <stdexcept>
# include <vector>
# include <atomic>
# include <string>

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

    class socket
    {
    public:
    };

} // !namespace bal

#endif // !_BAL_HH_INCLUDED
