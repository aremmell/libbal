#!/usr/bin/env bash

# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: Copyright (c) 2004-current Ryan M. Lederman

set -eu > /dev/null 2>&1

C_SRC="./src/*.c"
CXX_SRC="./sample/*.cc"
CC_JSON="./build/compile_commands.json"

CPUS="$(getconf _NPROCESSORS_ONLN 2> /dev/null || printf '%s\n' '1')" && \
    export CPUS

# $1 = message
echo_info()
{
    printf "\x1b[1;97mINFO: \x1b[0;97m%s\x1b[0m\n" "$1"
}

# $1 = message
echo_notice()
{
    printf "\x1b[1;33mNOTICE: \x1b[0;97m%s\x1b[0m\n" "$1"
}

# $1 = message
echo_error()
{
    printf "\x1b[1;91mERROR: \x1b[0;97m%s\x1b[0m\n" "$1"
}

# Returns 0 if the command exists, 1 otherwise
# $1 = command to test the existence of
test_command()
{
    if ! command -v "$1" >/dev/null 2>&1; then
        echo_notice "$1 not found"
        return 1
    fi

    return 0
}

rebuild_project()
{
    echo_info "[CMake] rebuilding project..."
    if ! cmake --preset debug >/dev/null || \
       ! cmake --build build --clean-first >/dev/null; then
       echo_error "failed to build project! fix error(s) and try again"
       return 1
    fi
    echo_info "[CMake] rebuild complete."
}

run_cppcheck()
{
    if test_command "cppcheck"; then {
        rebuild_project || return 1
        echo "Running cppcheck..."
        cppcheck \
            --check-level=exhaustive \
            --suppress=missingIncludeSystem \
            --max-ctu-depth=16 \
            -j "${CPUS:?}" \
            --force \
            --std=c11 \
            --enable=all \
            --language=c \
            --project="${CC_JSON}" \
        && \
        cppcheck \
            --check-level=exhaustive \
            --suppress=missingIncludeSystem \
            --suppress=cstyleCast \
            --max-ctu-depth=16 \
            -j "${CPUS:?}" \
            --force \
            --std=c++20 \
            --enable=all \
            --language=c++ \
            --project="${CC_JSON}"
        } || {
            echo_error "cppcheck failed"
            return 1
        }
    else
        echo_notice "skipping cppcheck"
    fi
}

run_clang_tidy()
{
    if test_command "clang-tidy"; then {
        echo "Running clang-tidy..."
        clang-tidy \
            -p build \
            --checks=clang-diagnostic-*,clang-analyzer-* \
            "${C_SRC}" && \
        clang-tidy \
            -p build --export-fixes=clang-fixes.yml \
            --checks=clang-diagnostic-*,clang-analyzer-*,modernize-*,readability-* \
            "${CXX_SRC}"
        } || {
            echo_error "clang-tidy returned error code $?"
            return $?
        }
    else
        echo_notice "skipping clang-tidy"
    fi
}

run_analyze_build()
{
    if test_command "analyze-build"; then
        rebuild_project || return 1
        echo "Running analyze-build..."
        if ! analyze-build \
            --cdb "${CC_JSON}" \
            --use-analyzer "$(which clang)" \
            --analyze-headers \
            --status-bugs ; then
            echo_error "analyze-build returned error code $?"
            return $?
        fi
    else
        echo_notice "skipping analyze-build"
    fi
}

run_pvs_studio()
{
    _pvs_tools=(
        "pvs-studio-analyzer"
        "plog-converter"
    )

    for _tool in "${_pvs_tools[@]}"; do
        if ! test_command "$_tool"; then
            echo_notice "$_tool not found; skippping PVS-Studio."
            _NO_PVS_STUDIO=1
            break
        fi
    done

    if [ -z "${_NO_PVS_STUDIO:-}" ]; then {
        rebuild_project || return 1
        echo "Running PVS-Studio..."
        rm -rf ./pvsreport
        rm -f ./log.pvs
        pvs-studio-analyzer analyze \
            --intermodular \
            -C clang \
            -f "${CC_JSON}" \
            -j "${CPUS:-1}" \
            -o log.pvs \
        && plog-converter \
            -a "GA:1,2,3" \
            -t fullhtml log.pvs \
            -o pvsreport > /dev/null
        } || {
            echo_error "PVS-Studio failed!"
            return 1
        }
        echo_info "PVS-Studio report available at ./pvsreport/index.html"
    else
        echo_notice "skipping PVS-Studio"
    fi
}

print_usage()
{
    printf \
    "\x1b[97mUsage: %s [cppcheck|clang-tidy|analyze-build|pvs-studio|all]\x1b[0m\n" \
    "$(basename "$0")"
    exit 1
}

_EXIT_CODE=0
_RUN_CPPCHECK=false
_RUN_CLANG_TIDY=false
_RUN_ANALYZE_BUILD=false
_RUN_PVS_STUDIO=false

_args=("$@")

if [ ${#_args[@]} -eq 0 ]; then
    echo_error "No arguments provided."
    print_usage
fi

for _arg in "${_args[@]}"; do
    case "$_arg" in
        cppcheck)
            _RUN_CPPCHECK=true
            ;;
        clang-tidy)
            _RUN_CLANG_TIDY=true
            ;;
        analyze-build)
            _RUN_ANALYZE_BUILD=true
            ;;
        pvs-studio)
            _RUN_PVS_STUDIO=true
            ;;
        all)
            _RUN_CPPCHECK=true
            _RUN_CLANG_TIDY=true
            _RUN_ANALYZE_BUILD=true
            _RUN_PVS_STUDIO=true
            ;;
        *)
            echo_error "Unknown argument '$_arg'"
            print_usage
            ;;
    esac
done

if [ ${_RUN_CPPCHECK} = true ]; then
    run_cppcheck || exit $?
fi

if [ ${_RUN_CLANG_TIDY} = true ]; then
    run_clang_tidy || exit $?
fi

if [ ${_RUN_ANALYZE_BUILD} = true ]; then
    run_analyze_build || exit $?
fi

if [ ${_RUN_PVS_STUDIO} = true ]; then
    run_pvs_studio || exit $?
fi
