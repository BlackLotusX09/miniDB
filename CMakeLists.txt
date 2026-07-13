cmake_minimum_required(VERSION 3.15)

project(MiniDB VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Shared storage library ─────────────────────────────────────────────────────
set(STORAGE_SOURCES
    src/storage/DiskManager.cpp
    src/storage/BufferPool.cpp
    src/storage/page.cpp
    src/storage/tuple.cpp
    src/storage/rid.cpp
    src/storage/TableHeap.cpp
    src/storage/TableIterator.cpp
)

# ── Main executable ────────────────────────────────────────────────────────────
add_executable(minidb
    src/main.cpp
    ${STORAGE_SOURCES}
)
target_include_directories(minidb PRIVATE include)

# ── Test: DiskManager + BufferPool ────────────────────────────────────────────
add_executable(disk_manager_test
    test/disk_manager_test.cpp
    src/storage/DiskManager.cpp
    src/storage/page.cpp
    src/storage/BufferPool.cpp
)
target_include_directories(disk_manager_test PRIVATE include)

# ── Test: SlottedPage ─────────────────────────────────────────────────────────
add_executable(slottedPage_test
    test/slottedPage_test.cpp
    src/storage/page.cpp
)
target_include_directories(slottedPage_test PRIVATE include)

# ── Test: Day 7 Full Integration ──────────────────────────────────────────────
add_executable(integration_test
    test/integration_test.cpp
    ${STORAGE_SOURCES}
)
target_include_directories(integration_test PRIVATE include)

# ── Copy data directory into build ────────────────────────────────────────────
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

enable_testing()
add_test(NAME IntegrationTest COMMAND integration_test)