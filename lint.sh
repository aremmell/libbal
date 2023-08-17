#!/usr/bin/env bash

if which cppcheck >/dev/null 2>&1; then
    echo "Running cppcheck..."
    cppcheck --check-level=exhaustive --max-ctu-depth=8 \
             --force --std=c11 --enable=all --language=c ./src/*.c && \
    cppcheck --check-level=exhaustive --max-ctu-depth=8 \
             --force --std=c++20 --enable=all --language=c++ ./sample/*.cc || {
        echo "cppcheck returned error code $?; exiting"
        exit $?
    }
else
    echo "cppcheck not found; skipping"
fi

if which clang-tidy >/dev/null 2>&1; then
    echo "Running clang-tidy..."
    clang-tidy -p build --checks=clang-diagnostic-*,clang-analyzer-* ./src/*.c &&
    clang-tidy -p build --export-fixes=clang-fixes.yml \
               --checks=clang-diagnostic-*,clang-analyzer-*,modernize-*,readability-* ./sample/*.cc || {
        echo "clang-tidy returned error code $?; exiting"
        exit $?
    }
else
    echo "clang-tidy not found; skipping"
fi

if which analyze-build >/dev/null 2>&1; then
    echo "Running analyze-build..."
    analyze-build --cdb build/compile_commands.json \
                  --use-analyzer $(which clang) \
                  --analyze-headers --status-bugs || {
        echo "analyze-build returned error code $?; exiting"
        exit $?
    }
else
    echo "analyze-build not found; skipping"
fi
