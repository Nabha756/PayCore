cmake_minimum_required(VERSION 3.16)
project(PayCore VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug)
endif()

option(PAYCORE_BUILD_TESTS "Build the GoogleTest unit tests" ON)

# Core logic as a library, so the app and the tests share the same code.
add_library(paycore_core
    src/Types.cpp
    src/Utils.cpp
    src/Transaction.cpp
    src/Payment.cpp
    src/Refund.cpp
    src/PaymentValidator.cpp
    src/PaymentProcessor.cpp
    src/IdGenerator.cpp
    src/TransactionManager.cpp
    src/FileRepository.cpp
    src/ConsoleApp.cpp
)
target_include_directories(paycore_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

if(MSVC)
    target_compile_options(paycore_core PRIVATE /W4)
else()
    target_compile_options(paycore_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

# GCC 8 needs an extra library for <filesystem>; newer compilers do not.
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9)
    target_link_libraries(paycore_core PUBLIC stdc++fs)
endif()

add_executable(paycore src/main.cpp)
target_link_libraries(paycore PRIVATE paycore_core)

if(PAYCORE_BUILD_TESTS)
    enable_testing()

    # Use an installed GoogleTest if there is one, otherwise download it.
    find_package(GTest QUIET)
    if(NOT GTest_FOUND)
        include(FetchContent)
               FetchContent_Declare(
            googletest
            URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
        )
        # Windows: keep GoogleTest on the same runtime library as our code.
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(googletest)
    endif()

    add_executable(paycore_tests
        tests/test_validator.cpp
        tests/test_transaction.cpp
        tests/test_processor.cpp
        tests/test_manager.cpp
        tests/test_persistence.cpp
    )
    target_link_libraries(paycore_tests PRIVATE paycore_core GTest::gtest_main)

    include(GoogleTest)
    gtest_discover_tests(paycore_tests)
endif()
