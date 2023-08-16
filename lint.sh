#!/usr/bin/env bash

if which cppcheck >/dev/null 2>&1; then
    echo "Running cppcheck..."
    cppcheck --check-level=exhaustive --max-ctu-depth=8 \
             --force --std=c11 --enable=all --language=c ./*.c && \
    cppcheck --check-level=exhaustive --max-ctu-depth=8 \
             --force --std=c++20 --enable=all --language=c++ ./*.cc || {
        echo "cppcheck returned error code $?; exiting"
        exit $?
    }
else
    echo "cppcheck not found; skipping"
fi

if which clang-tidy >/dev/null 2>&1; then
    echo "Running clang-tidy..."
    clang-tidy -p build --export-fixes=clang-fixes.yml \
               --checks=clang-diagnostic-*,clang-analyzer-* ./*.c &&
    clang-tidy -p build --export-fixes=clang-fixes.yml \
               --checks=clang-diagnostic-*,clang-analyzer-*,modernize-*,readability-* ./*.cc || {
        echo "clang-tidy returned error code $?; exiting"
        exit $?
    }

else
    echo "clang-tidy not found; skipping"
fi

