# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: Copyright (c) 2004-current Ryan M. Lederman

cmake_minimum_required(
    VERSION 3.12.0
)

# variables
set(PROJECT_NAME bal)
set(CLIENT_EXECUTABLE_NAME balclient)
set(SERVER_EXECUTABLE_NAME balserver)
set(TESTS_EXECUTABLE_NAME baltests)
set(TESTSXX_EXECUTABLE_NAME baltests++)
set(STATIC_LIBRARY_NAME bal_static)
set(SHARED_LIBRARY_NAME bal_shared)

# "-dev" if this is a development build, empty if it is a release build.
set(PROJECT_VERSION_SUFFIX -dev)

# true if this is a release build, false if it is a development build.
set(PROJECT_IS_REL_BUILD false)

# create compile commands for static analysis
set(CMAKE_EXPORT_COMPILE_COMMANDS true)

# set the C/C++ standards
if(${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.12")
    set(CMAKE_C_STANDARD 17)
    set(CMAKE_CXX_STANDARD 20)
else()
    message(FATAL_ERROR, "CMake >= 3.12 is required in order to build libbal")
endif()

set(CMAKE_C_STANDARD_REQUIRED TRUE)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

# policy settings, so CMake doesn't whine.
cmake_policy(SET CMP0025 NEW) # AppleClang
cmake_policy(SET CMP0048 NEW) # project VERSION
cmake_policy(SET CMP0065 NEW) # ENABLE_EXPORTS
cmake_policy(SET CMP0056 NEW) # CMAKE_EXE_LINKER_FLAGS
cmake_policy(SET CMP0066 NEW) # CMAKE_<LANG>_FLAGS_<BUILDTYPE>

if(${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.13")
    cmake_policy(SET CMP0077 NEW) # option
endif()

if(${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.17")
    cmake_policy(SET CMP0102 NEW) # mark_as_advanced
endif()

if(${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.21")
    cmake_policy(SET CMP0126 NEW) # cache
endif()

# define the project
project(
    ${PROJECT_NAME}
    VERSION 0.2.0
    LANGUAGES C CXX
    DESCRIPTION "Berkeley Abstraction Layer library"
)

# debug/release preprocessor
add_compile_definitions(
    $<$<CONFIG:Debug>:DEBUG>
    $<$<CONFIG:Debug>:BAL_DBGLOG> # general debug tracing
    #$<$<CONFIG:Debug>:BAL_DBGLOG_WARNERR_ONLY> # debug tracing of warnings and errors only.
    #$<$<CONFIG:Debug>:BAL_DBGLOG_SETERROR> # debug tracing on every internal error.
    #$<$<CONFIG:Debug>:BAL_DBGLOG_ASYNC_IO> # debug tracing for async I/O events.
    $<$<CONFIG:Release>:NDEBUG>
)

if (MSVC)
    add_compile_options(
        /W4 /MP /GS /experimental:c11atomics /wd4267
        $<$<CONFIG:Debug>:/Od>
        $<$<CONFIG:Debug>:/Zi>
        $<$<CONFIG:Release>:/Ox>
    )
else()
    set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
    set(THREADS_PREFER_PTHREAD_FLAG TRUE)
    find_package(Threads REQUIRED)

    # -Wformat-nonliteral is also an option
    add_compile_options(
        -Wall -Wextra -Wpedantic -Werror -Wdouble-promotion -Wconversion -Wno-sign-conversion -Wno-string-conversion -Wshift-overflow
        $<$<CONFIG:Debug>:-O0>
        $<$<CONFIG:Debug>:-g3>
        $<$<CONFIG:Release>:-O3>
    )

    if (CMAKE_COMPILER_IS_GNUCC)
        add_compile_options(
            -Wformat-truncation
        )
    endif()

    if (THREADS_HAVE_PTHREAD_ARG)
        add_compile_options(
            -pthread
        )
        add_link_options(
            -pthread
        )
    endif()

    add_compile_definitions(
        $<$<CONFIG:Release>:_FORTIFY_SOURCE=2>
    )
endif()

execute_process(
    COMMAND git rev-parse --short --verify HEAD
    OUTPUT_VARIABLE GIT_COMMIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ECHO_OUTPUT_VARIABLE
)

configure_file(
    include/bal/version.h.in
    include/bal/version.h
    NEWLINE_STYLE LF
)

add_executable(
    ${CLIENT_EXECUTABLE_NAME}
    sample/balclient.cc
    sample/balcommon.cc
)

add_executable(
    ${SERVER_EXECUTABLE_NAME}
    sample/balserver.cc
    sample/balcommon.cc
)

add_executable(
    ${TESTS_EXECUTABLE_NAME}
    tests/tests.c
    tests/tests_shared.c
)

add_executable(
    ${TESTSXX_EXECUTABLE_NAME}
    tests/tests++.cc
    tests/tests_shared.c
)

file(
    GLOB
    BAL_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c
)

add_library(
    ${STATIC_LIBRARY_NAME}
    STATIC
    ${BAL_SRC}
)

add_library(
    ${SHARED_LIBRARY_NAME}
    SHARED
    ${BAL_SRC}
)

if(!WIN32)
    set_target_properties(${STATIC_LIBRARY_NAME} PROPERTIES OUTPUT_NAME ${PROJECT_NAME})
    set_target_properties(${SHARED_LIBRARY_NAME} PROPERTIES OUTPUT_NAME ${PROJECT_NAME})
endif()

target_include_directories(
    ${STATIC_LIBRARY_NAME}
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_BINARY_DIR}/include
)

target_include_directories(
    ${SHARED_LIBRARY_NAME}
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_BINARY_DIR}/include
)

target_include_directories(
    ${TESTS_EXECUTABLE_NAME}
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_BINARY_DIR}/include
)

target_include_directories(
    ${TESTSXX_EXECUTABLE_NAME}
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_BINARY_DIR}/include
)

if(!WIN32)
    target_link_libraries(
        ${SERVER_EXECUTABLE_NAME}
        PUBLIC
        Threads::Threads
    )

    target_link_libraries(
        ${CLIENT_EXECUTABLE_NAME}
        PUBLIC
        Threads::Threads
    )

    target_link_libraries(
        ${TESTS_EXECUTABLE_NAME}
        PUBLIC
        Threads::Threads
    )
endif()

target_link_libraries(
    ${CLIENT_EXECUTABLE_NAME}
    ${STATIC_LIBRARY_NAME}
)

target_link_libraries(
    ${SERVER_EXECUTABLE_NAME}
    ${STATIC_LIBRARY_NAME}
)

target_link_libraries(
    ${TESTS_EXECUTABLE_NAME}
    ${STATIC_LIBRARY_NAME}
)

target_link_libraries(
    ${TESTSXX_EXECUTABLE_NAME}
    ${STATIC_LIBRARY_NAME}
)

target_compile_features(
    ${CLIENT_EXECUTABLE_NAME}
    PUBLIC
    ${CXX_STANDARD}
)

target_compile_features(
    ${SERVER_EXECUTABLE_NAME}
    PUBLIC
    ${CXX_STANDARD}
)

target_compile_features(
    ${TESTS_EXECUTABLE_NAME}
    PUBLIC
    ${C_STANDARD}
)

target_compile_features(
    ${TESTSXX_EXECUTABLE_NAME}
    PUBLIC
    ${C_STANDARD}
)

target_compile_features(
    ${TESTSXX_EXECUTABLE_NAME}
    PUBLIC
    ${CXX_STANDARD}
)

target_compile_features(
    ${STATIC_LIBRARY_NAME}
    PUBLIC
    ${C_STANDARD}
)

target_compile_features(
    ${SHARED_LIBRARY_NAME}
    PUBLIC
    ${C_STANDARD}
)

install(
    TARGETS ${STATIC_LIBRARY_NAME} ${SHARED_LIBRARY_NAME}
    DESTINATION lib
    PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
    CONFIGURATIONS Release
)

#file(
#    GLOB
#    BAL_HEADERS
#    ${CMAKE_CURRENT_SOURCE_DIR}/include/*/*.h
#)

#install(
#    FILES ${BAL_HEADERS} ${CMAKE_CURRENT_BINARY_DIR}/include/bal/version.h
#    DESTINATION include/bal
#    PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
#    CONFIGURATIONS Release
#)

#install(
#    FILES ${CMAKE_CURRENT_SOURCE_DIR}/include/bal.h
#    DESTINATION include
#    PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
#    CONFIGURATIONS Release
#)
