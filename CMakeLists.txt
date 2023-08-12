cmake_minimum_required(
    VERSION 3.0.0...3.27.1
)

# variables
set(PROJECT_NAME bal)
set(CLIENT_EXECUTABLE_NAME balclient)
set(SERVER_EXECUTABLE_NAME balserver)
set(STATIC_LIBRARY_NAME bal_static)
set(SHARED_LIBRARY_NAME bal_shared)

# "-dev" if this is a development build, empty if it is a release build.
set(PROJECT_VERSION_SUFFIX, "-dev")

# true if this is a development build, false if it is a release build.
set(PROJECT_IS_DEV_BUILD true)

# policy settings, so CMake doesn't whine.
cmake_policy(SET CMP0025 NEW) # AppleClang
cmake_policy(SET CMP0048 NEW) # project VERSION
cmake_policy(SET CMP0065 NEW) # ENABLE_EXPORTS
cmake_policy(SET CMP0126 NEW) # cache
cmake_policy(SET CMP0056 NEW) # CMAKE_EXE_LINKER_FLAGS
cmake_policy(SET CMP0066 NEW) # CMAKE_<LANG>_FLAGS_<BUILDTYPE>
cmake_policy(SET CMP0102 NEW) # mark_as_advanced

# toolchain-related
if (WIN32)
    set(C_FLAGS_BASE "/experimental:c11atomics")
    set(CMAKE_C_FLAGS_DEBUG "${C_FLAGS_BASE} ${CMAKE_C_FLAGS}")
    set(CMAKE_C_FLAGS_RELEASE "${C_FLAGS_BASE} ${CMAKE_C_FLAGS}")
    set(CMAKE_CXX_FLAGS_DEBUG "${C_FLAGS_BASE} /DDEBUG")
    set(CMAKE_CXX_FLAGS_RELEASE "${C_FLAGS_BASE} /DNDEBUG")
else()
    set(C_FLAGS_BASE "-Wall -Wextra -Wpedantic -Werror")
    set(CMAKE_C_FLAGS_DEBUG "${C_FLAGS_BASE} -g3 -O0 -DDEBUG")
    set(CMAKE_C_FLAGS_RELEASE "${C_FLAGS_BASE} -O3 -D_FORTIFY_SOURCE=2 -DNDEBUG")

    set(CXX_FLAGS_BASE "-Wall -Wextra -Wpedantic -Werror")
    set(CMAKE_CXX_FLAGS_DEBUG "${C_FLAGS_BASE} -g3 -O0 -DDEBUG")
    set(CMAKE_CXX_FLAGS_RELEASE "${C_FLAGS_BASE} -O3 -D_FORTIFY_SOURCE=2 -DNDEBUG")

    set(CMAKE_EXE_LINKER_FLAGS, "${CMAKE_EXE_LINKER_FLAGS} -pthread")
endif()

# define the project
project(
    ${PROJECT_NAME}
    VERSION 0.2.0
    LANGUAGES C CXX
    DESCRIPTION "Berkeley Abstraction Layer"
)

execute_process(
    COMMAND git rev-parse --short --verify HEAD
    OUTPUT_VARIABLE GIT_COMMIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ECHO_OUTPUT_VARIABLE
    COMMAND_ERROR_IS_FATAL ANY
)

configure_file(
    version.h.in
    include/version.h
    NEWLINE_STYLE LF
)

add_executable(
    ${CLIENT_EXECUTABLE_NAME}
    balclient.cc
    balcommon.cc
)

add_executable(
    ${SERVER_EXECUTABLE_NAME}
    balserver.cc
    balcommon.cc
)

add_library(
    ${STATIC_LIBRARY_NAME}
    STATIC
    bal.c
    balinternal.c
    balerrors.c
)

add_library(
    ${SHARED_LIBRARY_NAME}
    SHARED
    bal.c
    balinternal.c
    balerrors.c
)

if(!WIN32)
    set_target_properties(${STATIC_LIBRARY_NAME} PROPERTIES OUTPUT_NAME ${PROJECT_NAME})
    set_target_properties(${SHARED_LIBRARY_NAME} PROPERTIES OUTPUT_NAME ${PROJECT_NAME})
endif()

target_include_directories(
    ${STATIC_LIBRARY_NAME}
    PUBLIC
    .
    ${CMAKE_CURRENT_BINARY_DIR}/include
)

target_include_directories(
    ${SHARED_LIBRARY_NAME}
    PUBLIC
    .
    ${CMAKE_CURRENT_BINARY_DIR}/include
)

target_link_libraries(
    ${CLIENT_EXECUTABLE_NAME}
    ${STATIC_LIBRARY_NAME}
)

target_link_libraries(
    ${SERVER_EXECUTABLE_NAME}
    ${STATIC_LIBRARY_NAME}
)

target_compile_features(
    ${CLIENT_EXECUTABLE_NAME}
    PUBLIC
    cxx_std_20
)

target_compile_features(
    ${SERVER_EXECUTABLE_NAME}
    PUBLIC
    cxx_std_20
)

target_compile_features(
    ${STATIC_LIBRARY_NAME}
    PUBLIC
    c_std_17
)

target_compile_features(
    ${SHARED_LIBRARY_NAME}
    PUBLIC
    c_std_17
)

install(
    TARGETS ${STATIC_LIBRARY_NAME} ${SHARED_LIBRARY_NAME}
    DESTINATION lib
    PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
    CONFIGURATIONS Release
)

install(
    FILES bal.h include/version.h
    DESTINATION include/libbal
    PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
    CONFIGURATIONS Release
)
