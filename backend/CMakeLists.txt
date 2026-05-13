cmake_minimum_required(VERSION 3.22)

project(crdt_text_backend LANGUAGES CXX)

if(POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Boost REQUIRED COMPONENTS system)
find_package(nlohmann_json REQUIRED)

add_library(crdt_core
    src/crdt/TextCrdt.cpp
    src/server/CollaborativeDocument.cpp
    src/storage/DocumentStore.cpp
)

target_include_directories(crdt_core PUBLIC src)
target_link_libraries(crdt_core PUBLIC nlohmann_json::nlohmann_json)

add_executable(crdt_backend
    src/main.cpp
    src/server/WebSocketServer.cpp
)

target_link_libraries(crdt_backend PRIVATE crdt_core Boost::system pthread)

enable_testing()

add_executable(crdt_tests
    tests/TextCrdtTests.cpp
)

target_link_libraries(crdt_tests PRIVATE crdt_core)

add_test(NAME crdt_tests COMMAND crdt_tests)
