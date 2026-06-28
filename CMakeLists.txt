cmake_minimum_required(VERSION 3.20 FATAL_ERROR)

find_program(CCACHE_PROGRAM ccache)
if (CCACHE_PROGRAM)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
endif()

set(CPP_ORDERBOOK orderbook)
project(${CPP_ORDERBOOK} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)

set(CMAKE_THREAD_LIBS_INIT "-lpthread")
set(CMAKE_HAVE_THREADS_LIBRARY ON)
set(CMAKE_USE_PTHREADS_INIT ON)
set(THREADS_PREFER_PTHREAD_FLAG ON)

set(PROJECT_DIR ${CMAKE_CURRENT_SOURCE_DIR})

option(ENABLE_TESTING "Enable test target generation" OFF)

add_executable (main main.cpp)

set(CPM_USE_LOCAL_PACKAGES ON)
include(cmake/CPM.cmake)
CPMUsePackageLock(package-lock.cmake)

set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)

CPMAddPackage( NAME Boost VERSION 1.80.0 GITHUB_REPOSITORY "boostorg/boost" GIT_TAG "boost-1.80.0")
CPMAddPackage( NAME decimal VERSION 2.1.0 GITHUB_REPOSITORY "geseq/cpp-decimal" GIT_TAG "v2.1.0")
CPMAddPackage( NAME pool VERSION 0.6.1 GITHUB_REPOSITORY "geseq/cpp-pool" GIT_TAG "v0.5.0")
# cpp-fastchan defaults its own ENABLE_TESTING=ON and shares that option name
# with us, so add_subdirectory'ing it would pull its (slow) test suite into our
# ctest. Force ENABLE_TESTING OFF just for fastchan's configure, then restore our
# value for our own test block below. (We use add_subdirectory, not DOWNLOAD_ONLY,
# so _deps/fastchan-src/include is populated for the bench adapter build.)
set(_ob_enable_testing_save "${ENABLE_TESTING}")
set(ENABLE_TESTING OFF CACHE BOOL "" FORCE)
CPMAddPackage( NAME fastchan GITHUB_REPOSITORY "geseq/cpp-fastchan" GIT_TAG "main")
set(ENABLE_TESTING "${_ob_enable_testing_save}" CACHE BOOL "" FORCE)

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

    # Parallel targets that recompile the orderbook + determinism suites against
    # the RbTreeLevels backend (the default targets above cover ArrayLevels), so
    # BOTH level-store backends are exercised by the same assertions.
    FOREACH (rb_test orderbook_test.cpp determinism_test.cpp)
        set(rb_name ${rb_test}_rbtree)
        message("Adding test: " ${rb_name})
        add_executable(${rb_name} ${SOURCES} ${PROJECT_SOURCE_DIR}/test/${rb_test})
        target_compile_definitions(${rb_name} PRIVATE OB_TEST_LEVELS=::orderbook::RbTreeLevels)
        target_link_libraries(${rb_name} PRIVATE ${CMAKE_THREAD_LIBS_INIT} ${CPP_ORDERBOOK} gtest gtest_main Boost::algorithm Boost::intrusive decimal pool)
        add_test(${rb_name} ${rb_name})
        set_property(TEST ${rb_name} PROPERTY LABELS "test")
    ENDFOREACH ()
endif()

target_link_libraries(main PRIVATE ${CPP_ORDERBOOK} Boost::intrusive decimal pool)

# --- Link-time optimization (LTO/IPO) for optimized builds only. ------------
# Enabled only for Release-type configs and only when the toolchain supports
# it. Debug builds and the sanitizer builds (ASan/UBSan/TSan) are left
# untouched on purpose: LTO is slow under sanitizers and can misbehave. Note
# the sanitizer CI builds the engine via bench/build.sh, not this CMake, so it
# never reaches here; the gate below additionally keeps a Debug configure clean.
include(CheckIPOSupported)
check_ipo_supported(RESULT _ipo_ok OUTPUT _ipo_msg LANGUAGES CXX)
if (_ipo_ok AND CMAKE_BUILD_TYPE MATCHES "^(Release|RelWithDebInfo|MinSizeRel)$")
    set_property(TARGET ${CPP_ORDERBOOK} main PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
    message(STATUS "LTO/IPO enabled for ${CMAKE_BUILD_TYPE} build")
elseif (NOT _ipo_ok)
    message(STATUS "LTO/IPO not supported by toolchain, continuing without it: ${_ipo_msg}")
endif()
