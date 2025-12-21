# libbal

<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2004-2025 Ryan M. Lederman <lederman@gmail.com> -->

[![License](https://img.shields.io/github/license/aremmell/libbal?color=%2340b900&cacheSeconds=60)](https://github.com/aremmell/libbal/blob/master/LICENSE)
[![REUSE status](https://api.reuse.software/badge/github.com/aremmell/libbal)](https://api.reuse.software/info/github.com/aremmell/libbal)
[![Codacy Badge](https://app.codacy.com/project/badge/Grade/a7e6cfb38dc44542b34b9ce447fcc792)](https://app.codacy.com/gh/aremmell/libbal/dashboard?utm_source=gh&utm_medium=referral&utm_content=&utm_campaign=Badge_grade)
[![Security Rating](https://sonarcloud.io/api/project_badges/measure?project=aremmell_libbal&metric=security_rating)](https://sonarcloud.io/summary/new_code?id=aremmell_libbal)
[![Maintainability Rating](https://sonarcloud.io/api/project_badges/measure?project=aremmell_libbal&metric=sqale_rating)](https://sonarcloud.io/summary/new_code?id=aremmell_libbal)
[![Reliability Rating](https://sonarcloud.io/api/project_badges/measure?project=aremmell_libbal&metric=reliability_rating)](https://sonarcloud.io/summary/new_code?id=aremmell_libbal)

`libbal`: a lightweight, portable C17/C++20 abstraction layer for Berkeley Sockets, complete with asynchronous I/O and *no third-party depdendencies*.

## UPDATE December 2025

I'll be getting to documentation in the new year. No, I'm not going to use Claude, hipster.

I'm also considering branching off and converting everything to C23, since I've wanted to write `nullptr` in C for around 15 years. Yes, for that reason alone; wanna fight about it?

And finally, it looks like I have the "quirky" folks who maintain CMake to thank for wasting my time resolving various warnings that emerged from the ether a project whose source code hasn't changed&mdash;only the version of CMake on my machine. It's called backwards compatibility, people. Knock it off already. Nobody's ever going to use `-Wno-dev` unironically.

## UPDATE January 2025

I have set up a simplistic GitHub CMake action that will test compilation on Windows/Ubuntu/macOS with Clang/gcc/cl. This revealed several problems related to compiler versioning and portability, which are now fixed. I have tagged v0.3.0 as the first official releease of the reborn library!

## UPDATE December 2023

I have let this project slip as I got sidetracked with a few other things, but I intend fully to return to this in the new year and polish it up/stamp it for release.

## UPDATE October 2023

I have finished reorganizing things, and rewrote the C++ wrapper from scratch. I have also created a test suite for the core C library (`build/baltests[.exe]`) as well as a standalone test suite for the C++ wrapper (`build/baltests++[.exe]`). Next steps: documentation. This is something I've been wrestling with–I like Doxygen, but I have found that unless you wait until you ship to write the comments, they will change over time, entropy will creep in, and sooner or later your documentation is out of sync.

I want the in-IDE tooltip documentation to be available, so I will at least be writing Doxygen `@brief` descriptions for functions, types, etc. but [this MkDocs theme](https://squidfunk.github.io/mkdocs-material/) is too cool to not use. Therefore, I shall write the documentation by hand in markdown and use `mkdocs-material` to generate the static site.

I am going to use an example-driven approach to documentation, because that's the kind I like. Monkey see, monkey do. Speaking of that, I would like to demonstrate a small snippet showing the extent to which libbal encapsulates and abstracts away the horrors of Berkeley Sockets.

## A Little C++ Sample

```cpp
#include <bal.hh>
#include <iostream>
#include <cstdlib>

using namespace bal;
using namespace std;

int main(int argc, char** argv)
{
    /* this code is in a try/catch block, but exceptions are optional. */
    try {
        /* RAII library initializer. As long as it stays in scope, libbal stays ready for duty. */
        initializer balinit;

        /* RAII socket (this behavior is optional, and you can create a manual init/destroy socket as well). */
        scoped_socket client_sock {AF_INET, SOCK_STREAM, IPPROTO_TCP};

        /* the socket class uses public std::function properties for event handlers, so that you can use a lambda,
         * std::bind, or whatever. */
        client_sock.on_connect = [](scoped_socket* sock)
        {
            address peer_addr;
            sock->get_peer_addr(peer_addr);
            address_info addr_info = peer_addr.get_address_info();
            cout << "woo-hoo! connected to " << addr_info.get_addr() << " on port " << addr_info.get_port() << endl;
        };

        client_sock.on_write = [](scoped_socket* sock)
        {
            auto sent = sock->send(imaginary, imaginary.size(), MSG_NOSIGNAL);
            if (sent > 0) {
                cout << "wrote " << sent << " bytes " << endl;
            }
            // ...
        };

        client_sock.on_read = [](scoped_socket* sock)
        {
            // ...
        };

        // ... more events

        /* registers client_sock to recieve asynchronous I/O callbacks to its event handlers.
         * no need to `select` or `poll` here! */
        client_sock.async_poll(BAL_EVT_CLIENT);

        /* we can connect asynchronously now, and `on_connect` will be called when the connection is established. */
        client_sock.connect(imaginary_host, imaginary_port);

        cout << "running..." << endl;

        /* for the purposes of this demo, spin the main thread while it waits for events. */
        do {
            bal_sleep_msec(100);
            bal_thread_yield();
        } while (!should_quit());

        return EXIT_SUCCESS;
    } catch (bal::exception& ex) {
        cerr << "exception caught: " << ex.what() << "!" << endl;
        return EXIT_FAILURE;
    }
}
```

That's it! A nearly complete bare-bones TCP/IP client using libbal abstraction magic, and at this moment will compile and run on Linux, macOS, FreeBSD, and Windows.

Stay tuned for more, or just dig in if you are up for an undocumented challenge. -- RML

## UPDATE August 2023

I originally inteded to simply clean up the code, rename it, and add some documentation, but I have ended up making radical changes, including rewriting the async I/O mechanism. I'll be adding libbal to a strong CI pipeline as soon as I write a test suite.

I also tossed the C++ wrapper because I only had the header; the .cpp file was AWOL. I'll be writing that from scratch using C++20 as the standard.

## UPDATE July 2023

SAL is now BAL. There are too many things in computing called SAL, and it's a dude's name. I'm not a huge fan of BAL, but hey, libbal. It could work.

I have been in the mood to revist old projects that I wrote when I was younger, and I'm on to this one now, so I'm going to gussy it up, put a nice red dress on it (and matching lipstick) so that I can feel proud of it. Twenty years have passed since I started on this code, so hopefully it'll be 2 decades better when I'm done with it.

Stay tuned.
