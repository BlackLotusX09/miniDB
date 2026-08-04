cmake_minimum_required(VERSION 3.15)

project(MiniDB VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Shared source lists ────────────────────────────────────────────────────────
set(STORAGE_SOURCES
    src/storage/DiskManager.cpp
    src/storage/BufferPool.cpp
    src/storage/page.cpp
    src/catalog/tuple.cpp
    src/catalog/catalog.cpp
    src/catalog/schema.cpp
    src/storage/rid.cpp
    src/storage/TableHeap.cpp
    src/storage/TableIterator.cpp
)
set(PARSER_SOURCES
    src/parser/lexer.cpp
    src/parser/parser.cpp
)

set(BINDER_SOURCES
    src/binder/binder.cpp
)

set(BTREE_SOURCES
    src/storage/DiskManager.cpp
    src/storage/BufferPool.cpp
    src/storage/page.cpp
    src/storage/rid.cpp
    src/storage/b_tree.cpp
    src/storage/b_tree_leaf_page.cpp
    src/storage/b_tree_internal_page.cpp
)

# ── Copy data directory into build ────────────────────────────────────────────
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

enable_testing()

# ── Main executable ────────────────────────────────────────────────────────────
add_executable(minidb
    src/main.cpp
    ${STORAGE_SOURCES}
    ${PARSER_SOURCES}
    ${BINDER_SOURCES}
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
add_test(NAME disk_manager_test COMMAND disk_manager_test)

# ── Test: SlottedPage ─────────────────────────────────────────────────────────
add_executable(slotted_page_test
    test/slotted_page_test.cpp
    src/storage/page.cpp
)
target_include_directories(slotted_page_test PRIVATE include)
add_test(NAME slotted_page_test COMMAND slotted_page_test)

# ── Test: Full storage integration (TableHeap + TableIterator) ────────────────
add_executable(integration_test
    test/integration_test.cpp
    ${STORAGE_SOURCES}
)
target_include_directories(integration_test PRIVATE include)
add_test(NAME integration_test COMMAND integration_test)

# ── Test: B+ Tree — insert / delete / range scan / pin audit / restart ────────
add_executable(bplus_tree_test
    test/bplus_tree_test.cpp
    ${BTREE_SOURCES}
)
target_include_directories(bplus_tree_test PRIVATE include)
add_test(NAME bplus_tree_test COMMAND bplus_tree_test)

# ── Benchmark: B+ Tree vs sequential scan (Day 17) ────────────────────────────
add_executable(btree_benchmark
    test/btree_benchmark.cpp
    ${BTREE_SOURCES}
    src/catalog/tuple.cpp
    src/storage/TableHeap.cpp
    src/storage/TableIterator.cpp
)
target_include_directories(btree_benchmark PRIVATE include)
target_compile_options(btree_benchmark PRIVATE -O2)

# ── Test: Catalog — create / get / persistence round-trip ─────────────────────
add_executable(catalog_test
    test/catalog_test.cpp
    ${STORAGE_SOURCES}
)
target_include_directories(catalog_test PRIVATE include)
add_test(NAME catalog_test COMMAND catalog_test)

add_executable(lexer_test
    test/lexer_test.cpp
    ${PARSER_SOURCES}
)
target_include_directories(lexer_test PRIVATE include)
add_test(NAME lexer_test COMMAND lexer_test)

# ── Test: Binder — semantic validation ────────────────────────────────────────
add_executable(binder_test
    test/binder_test.cpp
    ${STORAGE_SOURCES}
    ${PARSER_SOURCES}
    ${BINDER_SOURCES}
)
target_include_directories(binder_test PRIVATE include)
add_test(NAME binder_test COMMAND binder_test)

add_executable(binder_debug
    test/binder_debug.cpp
    ${STORAGE_SOURCES}
    ${PARSER_SOURCES}
    ${BINDER_SOURCES}
)
target_include_directories(binder_debug PRIVATE include)
