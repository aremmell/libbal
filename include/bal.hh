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
# include <atomic>
# include <string>

namespace bal
{
    // TODO: capture extended error info and code, expose those.
    class exception : public std::runtime_error
    {
    public:
        using std::runtime_error::runtime_error;
        explicit exception(const bal_error& err) : exception(err.message) { }

        static exception from_last_error() {
            bal_error err {};
            [[maybe_unused]] int code = bal_get_error(&err);
            return exception(err);
        }
    };

    template<bool DNS>
    class address_info_base
    {
    public:
        address_info_base() = default;
        explicit address_info_base(const bal_sockaddr& addr) {
            from_sockaddr(addr);
        }
        virtual ~address_info_base() = default;

        address_info_base& operator=(const bal_sockaddr& addr) {
            return from_sockaddr(addr);
        }

        address_info_base& from_sockaddr(const bal_sockaddr& addr) {
            bal_addrstrings strings {};
            if (!bal_get_addrstrings(&addr, DNS, &strings)) {
                throw exception::from_last_error();
            }

            _type = strings.type;
            _host = strings.host;
            _addr = strings.addr;
            _port = strings.port;

            return *this;
        }

        std::string get_type() const {
            return _type;
        }

        std::string get_host() const {
            return _host;
        }

        // TODO: ability to return sockaddr
        std::string get_addr() const {
            return _addr;
        }

        // TODO: ability to return uint16_t
        std::string get_port() const {
            return _port;
        }

        void clear() {
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

    using dns_address_info = address_info_base<true>;
    using address_info = address_info_base<false>;

} // !namespace bal


#endif // !_BAL_HH_INCLUDED
