cmake_minimum_required(VERSION 3.20 FATAL_ERROR)

set(CPP_ORDERBOOK orderbook)
project(${CPP_ORDERBOOK} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_COMPILER "/usr/bin/clang++")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -std=c++20 -stdlib=libc++ -lc++abi")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++20 -stdlib=libc++")


set(CMAKE_THREAD_LIBS_INIT "-lpthread")
set(CMAKE_HAVE_THREADS_LIBRARY ON)
set(CMAKE_USE_PTHREADS_INIT ON)
set(THREADS_PREFER_PTHREAD_FLAG ON)

set(PROJECT_DIR ${CMAKE_CURRENT_SOURCE_DIR})

option(ENABLE_TESTING "Enable test target generation" OFF)

add_executable (main main.cpp)

include(cmake/CPM.cmake)
CPMUsePackageLock(package-lock.cmake)

set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)

CPMAddPackage( NAME Boost VERSION 1.80.0 GITHUB_REPOSITORY "boostorg/boost" GIT_TAG "boost-1.80.0")
CPMAddPackage( NAME decimal VERSION 2.1.0 GITHUB_REPOSITORY "geseq/cpp-decimal" GIT_TAG "v2.1.0")
CPMAddPackage( NAME pool VERSION 0.6.1 GITHUB_REPOSITORY "geseq/cpp-pool" GIT_TAG "v0.5.0")

include_directories(decimal_SOURCE_DIR)
include_directories(pool_SOURCE_DIR)

file(GLOB SOURCES "src/*.cpp")

include_directories(include)
add_library(${CPP_ORDERBOOK} STATIC ${SOURCES})
target_sources(${CPP_ORDERBOOK} INTERFACE include/orderbook.hpp)
target_include_directories(${CPP_ORDERBOOK} INTERFACE include/)
target_link_libraries(${CPP_ORDERBOOK} PRIVATE Boost::intrusive decimal pool)

if (ENABLE_TESTING)
    CPMAddPackage( NAME googletest GITHUB_REPOSITORY google/googletest VERSION 1.14.0 )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    include_directories(googletest_SOURCE_DIR)
    enable_testing()

    FILE(GLOB tests CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/test/*)
    FOREACH (test ${tests})
        get_filename_component(test_name ${test} NAME)
        message("Adding test: " ${test_name})
        add_executable(${test_name} ${SOURCES} ${PROJECT_SOURCE_DIR}/test/${test_name})
        target_link_libraries(${test_name} PRIVATE ${CMAKE_THREAD_LIBS_INIT} ${CPP_ORDERBOOK} gtest gtest_main Boost::algorithm Boost::intrusive decimal pool)
        add_test(${test_name} ${test_name})
        set_property(TEST ${test_name} PROPERTY LABELS "test")
    ENDFOREACH ()
endif()

target_link_libraries(main PRIVATE Boost::intrusive decimal pool)
