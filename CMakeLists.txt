cmake_minimum_required(VERSION 3.16)
project(crdt_editor LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(-Wall -Wextra -Wpedantic)

include(FetchContent)

# Cache downloads outside build/ so they survive cmake --fresh / deleting build/.
set(FETCHCONTENT_BASE_DIR "${CMAKE_SOURCE_DIR}/.cmake-deps" CACHE PATH "" FORCE)
# Never re-fetch if the source directory already exists.
set(FETCHCONTENT_UPDATES_DISCONNECTED ON)

# ---------------------------------------------------------------------------
# nlohmann/json (header-only)
# ---------------------------------------------------------------------------
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(nlohmann_json)

# ---------------------------------------------------------------------------
# uSockets — C companion library required by uWebSockets
# ---------------------------------------------------------------------------
FetchContent_Declare(
    usockets
    GIT_REPOSITORY https://github.com/uNetworking/uSockets.git
    GIT_TAG        v0.8.8
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(usockets)

file(GLOB USOCKETS_SOURCES
    ${usockets_SOURCE_DIR}/src/*.c
    ${usockets_SOURCE_DIR}/src/eventing/*.c
)
add_library(uSockets STATIC ${USOCKETS_SOURCES})
target_include_directories(uSockets PUBLIC ${usockets_SOURCE_DIR}/src)
# Disable SSL to avoid the OpenSSL/wolfSSL dependency.
target_compile_definitions(uSockets PUBLIC LIBUS_NO_SSL)
find_package(Threads REQUIRED)
target_link_libraries(uSockets PUBLIC Threads::Threads)

# ---------------------------------------------------------------------------
# uWebSockets (header-only, depends on uSockets)
# ---------------------------------------------------------------------------
FetchContent_Declare(
    uwebsockets
    GIT_REPOSITORY https://github.com/uNetworking/uWebSockets.git
    GIT_TAG        v20.62.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(uwebsockets)

add_library(uWebSockets INTERFACE)
target_include_directories(uWebSockets INTERFACE ${uwebsockets_SOURCE_DIR}/src)
target_link_libraries(uWebSockets INTERFACE uSockets)

# ---------------------------------------------------------------------------
# Shared CRDT sources (used by both server and tests)
# ---------------------------------------------------------------------------
file(GLOB_RECURSE SHARED_SOURCES
    shared/crdt/*.cpp
    shared/server/*.cpp
    shared/protocol/*.cpp
)

# ---------------------------------------------------------------------------
# Main server executable
# ---------------------------------------------------------------------------
add_executable(crdt_server backend/main.cpp ${SHARED_SOURCES})
target_include_directories(crdt_server PRIVATE shared)
target_link_libraries(crdt_server PRIVATE
    nlohmann_json::nlohmann_json
    uWebSockets
)

# ---------------------------------------------------------------------------
# Tests (opt-in via -DBUILD_TESTS=ON)
# ---------------------------------------------------------------------------
option(BUILD_TESTS "Build the Catch2 test suite" OFF)

if(BUILD_TESTS)
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        v3.5.4
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(Catch2)

    enable_testing()

    file(GLOB TEST_SOURCES shared/tests/*.cpp)
    if(TEST_SOURCES)
        add_executable(crdt_tests ${TEST_SOURCES} ${SHARED_SOURCES})
        target_include_directories(crdt_tests PRIVATE shared)
        target_link_libraries(crdt_tests PRIVATE
            Catch2::Catch2WithMain
            nlohmann_json::nlohmann_json
        )
        include(Catch)
        catch_discover_tests(crdt_tests)
    else()
        message(STATUS "BUILD_TESTS=ON but no test files found in backend/tests/")
    endif()
endif()
