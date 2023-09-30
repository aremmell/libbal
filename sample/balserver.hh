/*
 * balserver.hh
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
#ifndef _BAL_SERVER_HH_INCLUDED
# define _BAL_SERVER_HH_INCLUDED

# include "balcommon.hh"
# include <map>

namespace bal::balserver
{
    class client
    {
    public:
        client() = default;
        explicit client(bal_socket* s, const bal_sockaddr& addr) : _addrinfo(addr), _s(s) { }
        ~client() {
            if (_s != nullptr) {
                _bal_dbglog("closing and destroying socket " BAL_SOCKET_SPEC, _s->sd);
                [[maybe_unused]] bool closed = bal_close(&_s, true);
            }
        }

        client& operator=(client&& other) {
            _s = other._s;
            other._s = nullptr;
            _addrinfo = other._addrinfo;
            other._addrinfo.clear();
            return *this;
        }

        address_info get_address_info() const {
            return _addrinfo;
        }

        bal_socket* get_socket() const noexcept {
            return _s;
        }

    private:
        address_info _addrinfo {};
        bal_socket* _s = nullptr;
    };

    using client_map = std::map<bal_descriptor, client>;

    client* get_existing_client(bal_descriptor sd);
    void rem_existing_client(bal_descriptor sd);
    void on_client_connect(bal_socket* s);
    void on_client_disconnect(bal_socket* client_socket, bool error);

    void async_events_cb(bal_socket* s, uint32_t events);
} // !namespace balserver

#endif // !_BAL_SERVER_HH_INCLUDED
