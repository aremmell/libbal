/*
 * balhelpers.c
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
#include "bal/helpers.h"
#include "bal.h"

int _bal_aitoal(struct addrinfo* ai, bal_addrlist* out)
{
    int r = BAL_FALSE;

    if (_bal_validptr(ai) && _bal_validptr(out)) {
        struct addrinfo* cur = ai;
        bal_addr** a         = &out->addr;
        r                    = BAL_TRUE;

        do {
            *a = calloc(1UL, sizeof(bal_addr));
            if (!_bal_validptr(*a)) {
                _bal_handlelasterr();
                r = BAL_FALSE;
                break;
            }

            memcpy(&(*a)->addr, cur->ai_addr, cur->ai_addrlen);

            a   = &(*a)->next;
            cur = cur->ai_next;
        } while (NULL != cur);

        bal_resetaddrlist(out);
    }

    return r;
}

void _bal_socket_print(const bal_socket* s)
{
    if (_bal_validptr(s)) {
        printf("%p:\n{\n  sd = "BAL_SOCKET_SPEC"\n  addr_fam = %d\n  type = %d\n"
               "  proto = %d\nstate =\n  {\n  mask = %"PRIx32"\n  proc = %p\n  }"
               "\n}\n", (void*)s, s->sd, s->addr_fam, s->type, s->proto,
               s->state.mask, (void*)s->state.proc);
    } else {
        printf("<null>\n");
    }
}
