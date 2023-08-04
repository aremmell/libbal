cmake_minimum_required(VERSION 3.24.0)

# variables
set(PROJECT_NAME bal)

# policy settings, so CMake doesn't whine.
cmake_policy(SET CMP0048 NEW) # project VERSION
cmake_policy(SET CMP0065 NEW) # ENABLE_EXPORTS
cmake_policy(SET CMP0126 NEW) # cache
cmake_policy(SET CMP0056 NEW) # CMAKE_EXE_LINKER_FLAGS
cmake_policy(SET CMP0066 NEW) # CMAKE_<LANG>_FLAGS_<BUILDTYPE>
cmake_policy(SET CMP0102 NEW) # mark_as_advanced

# toolchain-related
set(C_FLAGS_BASE "-std=c11 -Wall -Wextra -Wpedantic -Werror -fPIC")
set(CMAKE_C_FLAGS_DEBUG "${C_FLAGS_BASE} -g3 -O0 -DDEBUG")
set(CMAKE_C_FLAGS_RELEASE "${C_FLAGS_BASE} -O3 -D_FORTIFY_SOURCE=2 -DNDEBUG")

# define the project
project(
    ${PROJECT_NAME}
    VERSION 0.2.0
    LANGUAGES C CXX
    DESCRIPTION "Berkeley Abstraction Layer"
)

#execute_process(
#    COMMAND git rev-parse --short --verify HEAD
#    OUTPUT_VARIABLE GIT_COMMIT_HASH
#    OUTPUT_STRIP_TRAILING_WHITESPACE
#    ECHO_OUTPUT_VARIABLE
#    COMMAND_ERROR_IS_FATAL ANY
#)

#configure_file(
#    version.h.in
#    include/version.h
#    NEWLINE_STYLE LF
#)

add_executable(
    balcli
    main.cc
)

add_library(
    bal
    STATIC
    bal.c
)

if(WIN32)
    add_library(libcmt.lib STATIC IMPORTED)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
endif()

target_include_directories(
    ${PROJECT_NAME}
    PUBLIC
    .
)

target_compile_features(
    ${PROJECT_NAME}
    PUBLIC
    c_std_11
)

#install(
#    PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}
#    TYPE BIN
#    CONFIGURATIONS Release
#)
