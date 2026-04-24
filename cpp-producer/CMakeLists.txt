cmake_minimum_required(VERSION 3.16)
project(cpp-producer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Release)

find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)

pkg_check_modules(RDKAFKA REQUIRED IMPORTED_TARGET rdkafka++)
pkg_check_modules(SPDLOG REQUIRED IMPORTED_TARGET spdlog)

include(FetchContent)
FetchContent_Declare(
    ixwebsocket
    GIT_REPOSITORY https://github.com/machinezone/IXWebSocket.git
    GIT_TAG        v11.4.5
)
set(USE_TLS ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(ixwebsocket)

find_path(RAPIDJSON_INCLUDE_DIR rapidjson/document.h)

add_executable(cpp-producer
    src/main.cpp
    src/kafka_producer.cpp
    src/tick_parser.cpp
)

target_include_directories(cpp-producer PRIVATE
    ${RAPIDJSON_INCLUDE_DIR}
    src/
)

target_link_libraries(cpp-producer PRIVATE
    ixwebsocket
    PkgConfig::RDKAFKA
    PkgConfig::SPDLOG
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
)

target_compile_options(cpp-producer PRIVATE -O3 -Wall -Wextra)
