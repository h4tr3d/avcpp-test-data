cmake_minimum_required(VERSION 3.19)

project(avcpp-test-data VERSION 1.0.0 LANGUAGES NONE)

set(AVCPP_TEST_DATA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/media"
    CACHE INTERNAL "Directory with avcpp test fixtures")

add_library(avcpp_test_data INTERFACE)
add_library(avcpp::test_data ALIAS avcpp_test_data)

target_include_directories(avcpp_test_data INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_definitions(avcpp_test_data
    INTERFACE AVCPP_TEST_DATA_DIR="${AVCPP_TEST_DATA_DIR}")
