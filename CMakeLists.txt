cmake_minimum_required(VERSION 3.15)

project(MiniDB VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(
    minidb
    src/main.cpp
    src/storage/DiskManager.cpp
)

target_include_directories(
    minidb
    PRIVATE include
)

add_executable(
    disk_manager_test
    
    test/disk_manager_test.cpp
    src/storage/DiskManager.cpp
    src/storage/page.cpp
)
add_executable(
    slottedPage_test
    test/slottedPage_test.cpp
    src/storage/page.cpp
)

target_include_directories(
    disk_manager_test
    PRIVATE include
)
target_include_directories(
    slottedPage_test
    PRIVATE include
)

file(
    COPY ${CMAKE_CURRENT_SOURCE_DIR}/data
    DESTINATION ${CMAKE_CURRENT_BINARY_DIR}
)

enable_testing()