# libbal

![Codacy grade](https://img.shields.io/codacy/grade/a7e6cfb38dc44542b34b9ce447fcc792?logo=codacy)
 [![REUSE status](https://api.reuse.software/badge/github.com/aremmell/libbal)](https://api.reuse.software/info/github.com/aremmell/libbal)

libbal: a portable C/C++ wrapper for Berkeley sockets (w/ IPv6 support)

<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2004-current Ryan M. Lederman <lederman@gmail.com> -->

## UPDATE August 2023

I originally inteded to simply clean up the code, rename it, and add some documentation, but I have ended up making radical changes, including rewriting the async I/O mechanism. I'll be adding libbal to a strong CI pipeline as soon as I write a test suite.

I also tossed the C++ wrapper because I only had the header; the .cpp file was AWOL. I'll be writing that from scratch using C++20 as the standard.

## UPDATE July 2023

SAL is now BAL. There are too many things in computing called SAL, and it's a dude's name. I'm not a huge fan of BAL, but hey, libbal. It could work.

I have been in the mood to revist old projects that I wrote when I was younger, and I'm on to this one now, so I'm going to gussy it up, put a nice red dress on it (and matching lipstick) so that I can feel proud of it. Twenty years have passed since I started on this code, so hopefully it'll be 2 decades better when I'm done with it.

Stay tuned.
