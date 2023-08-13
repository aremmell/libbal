#!/usr/bin/env bash

cppcheck -j 8 --check-level=exhaustive --max-ctu-depth=4 --force --std=c11 --enable=all --language=c *.c && \
cppcheck -j 8 --check-level=exhaustive --max-ctu-depth=4 --force --std=c++20 --enable=all --language=c++ *.cc

#clang-tidy -p build --use-color --header-filter=.* *.c *.cc
#analyze-build --cdb build/compile_commands.json --use-analyzer /usr/bin/clang --stats
