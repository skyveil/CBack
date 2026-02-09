cmake_minimum_required(VERSION 3.20)
project(cback VERSION 0.1 LANGUAGES C)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(BUILD_TESTS "Build tests" ON)

if (BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

add_library(cback SHARED src/alloc.c src/net.c src/collections.c)

find_package(OpenSSL REQUIRED)

target_link_libraries(cback OpenSSL::SSL OpenSSL::Crypto)

# NOTE: public for now, will work on visibility changes later
target_include_directories(cback PUBLIC include)
target_include_directories(cback PRIVATE src)

configure_file(cback.pc.in cback.pc @ONLY)
