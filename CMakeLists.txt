cmake_minimum_required(VERSION 3.20 FATAL_ERROR)

project("orderbook" LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_COMPILER "/usr/bin/clang++")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -std=c++17 -stdlib=libc++ -lc++abi")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -stdlib=libc++")


set(CMAKE_THREAD_LIBS_INIT "-lpthread")
set(CMAKE_HAVE_THREADS_LIBRARY ON)
set(CMAKE_USE_PTHREADS_INIT ON)
set(THREADS_PREFER_PTHREAD_FLAG ON)

set(PROJECT_DIR ${CMAKE_CURRENT_SOURCE_DIR})


add_executable (main main.cpp)

include(cmake/CPM.cmake)
CPMUsePackageLock(package-lock.cmake)

set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)
CPMAddPackage( NAME Boost VERSION 1.80.0 GITHUB_REPOSITORY "boostorg/boost" GIT_TAG "boost-1.80.0")

CPMAddPackage( NAME udecimal VERSION 0.5.1 GITHUB_REPOSITORY "geseq/cpp-udecimal" GIT_TAG "v0.5.1")

include_directories(udecimal_SOURCE_DIR)

target_link_libraries(main PRIVATE Boost::intrusive udecimal)
