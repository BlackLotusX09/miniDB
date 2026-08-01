# Day 2 — Design and Implement the Catalog

## Overview

The Catalog is the database's own metadata store. It maps every table name → `TableInfo` (Schema + `TableHeap*` + `BPlusTree*`), and it must survive process restarts by serializing itself to **page 0** of the `.db` file.

The skeleton in `include/catalog/catalog.h` and the surrounding storage layer are already in place. Everything listed below needs to be **written from scratch or wired together**.

---

## Task List

### Task 1 — Reserve Page 0 as the Catalog Page in `DiskManager`

**File:** [`DiskManager.cpp`](file:///Users/jaswanth/Desktop/miniDB/src/storage/DiskManager.cpp) · [`DiskManager.h`](file:///Users/jaswanth/Desktop/miniDB/include/storage/DiskManager.h)

**What to do:**
- On a **fresh database** (no `.meta` file), force `next_page_id = 1` instead of `0` so that page 0 is never handed out by `AllocatePages()`.
- On an **existing database**, the loaded `next_page_id` will already be ≥ 1 (page 0 was allocated when the catalog first initialized), so no change is needed for the re-open path.
- Add a helper `ReadCatalogPage(char* buf)` / `WriteCatalogPage(const char* buf)` that simply calls `ReadPage(0, buf)` / `WritePage(0, buf)` — keeps call sites in `Catalog` clean.

> **Why:** `AllocatePages()` currently starts at 0, so the very first `NewPage()` call would overwrite the catalog. Reserving page 0 up-front prevents this collision.

---

### Task 2 — Implement `Catalog` Constructor (Load from Disk)

**File:** [`src/catalog/catalog.cpp`](file:///Users/jaswanth/Desktop/miniDB/src/catalog/catalog.cpp) *(new file)*

**What to do:**
```cpp
Catalog::Catalog(BufferPoolManager* bpm) : bpm_(bpm) {
    LoadFromDisk();   // deserializes page 0 into tables_
}
```

**`LoadFromDisk()` steps:**
1. Call `bpm_->FetchPage(0)` to bring the catalog page into the buffer pool.
2. Read the raw bytes from `page->GetData()`.
3. Read `uint16_t num_tables` from offset 0.
4. For each table entry, deserialize (see Task 3 for the exact binary layout).
5. Reconstruct `TableInfo`:
   - Set `name`, `schema`, `first_page_id`, `index_root_page_id` from the serialized bytes.
   - Re-create `heap = std::make_unique<TableHeap>(bpm_, first_page_id)` using the **two-arg constructor** (reopen existing).
   - Leave `index = nullptr` for now (Phase 3/4 wires up the B-tree).
6. Insert into `tables_[name]`.
7. Unpin page 0 with `is_dirty = false`.

> **Edge case:** If page 0 is all-zeros (fresh DB), `num_tables` will be 0 — just return with an empty map.

---

### Task 3 — Define the Binary Serialization Layout

**Location:** Document in `catalog.cpp` comments or a small inline helper.

**Fixed binary layout for page 0:**

```
[ uint16_t  num_tables ]                     ← 2 bytes

For each table:
  [ uint8_t  name_len      ]                 ← 1 byte
  [ char     name[name_len]]                 ← name_len bytes
  [ page_id_t first_page_id]                 ← 4 bytes
  [ page_id_t index_root   ]                 ← 4 bytes  (INVALID_PAGE_ID if no index)
  [ uint8_t  col_count     ]                 ← 1 byte
  For each column:
    [ uint8_t  type        ]                 ← 1 byte  (0=INT,1=BOOL,2=VARCHAR)
    [ uint32_t max_length  ]                 ← 4 bytes (used for VARCHAR)
    [ uint8_t  col_name_len]                 ← 1 byte
    [ char     col_name[]  ]                 ← col_name_len bytes
```

Total per table ≈ ~30–80 bytes. A 4 KB page can hold hundreds of table entries.

> **No variable-length tricks:** every length-prefixed string is preceded by a 1-byte length field. No null terminators needed.

---

### Task 4 — Implement `Catalog::SaveToDisk()`

**File:** `src/catalog/catalog.cpp`

**What to do:**
1. `FetchPage(0)` — get the catalog page from the buffer pool.
2. Zero out the entire 4 KB buffer (`memset(data, 0, PAGE_SIZE)`).
3. Serialize all entries in `tables_` using the layout from Task 3.
4. Unpin page 0 with `is_dirty = true`.
5. Call `bpm_->flushAllPages()` (or a targeted flush of page 0) so data reaches disk immediately.

> **Rename note:** The header declares `saveToDisk()` (camelCase); rename it to `SaveToDisk()` in both header and implementation to match the Day 2 spec and the rest of the codebase convention.

---

### Task 5 — Implement `Catalog::CreateTable()`

**File:** `src/catalog/catalog.cpp`

**What to do:**
```cpp
void Catalog::CreateTable(const std::string& name, const Schema& schema) {
    // 1. Guard against duplicate names
    if (tables_.count(name)) return; // or throw

    // 2. Allocate a new TableHeap (its constructor calls NewPage internally)
    auto heap = std::make_unique<TableHeap>(bpm_);
    page_id_t first_pid = heap->GetFirstPageId();

    // 3. Build TableInfo and insert into the in-memory map
    TableInfo info;
    info.name              = name;
    info.schema            = schema;
    info.first_page_id     = first_pid;
    info.index_root_page_id = INVALID_PAGE_ID;
    info.heap              = std::move(heap);
    info.index             = nullptr;

    tables_[name] = std::move(info);

    // 4. Persist the catalog immediately
    SaveToDisk();
}
```

> **Why persist immediately:** If the process crashes after `CreateTable` but before the next explicit save, the table would be lost. Writing to page 0 right away is cheap and safe.

---

### Task 6 — Implement `Catalog::GetTable()`

**File:** `src/catalog/catalog.cpp`

**What to do:**
```cpp
TableInfo* Catalog::GetTable(const std::string& name) {
    auto it = tables_.find(name);
    if (it == tables_.end()) return nullptr;
    return &it->second;
}
```

Simple map lookup — returns a non-owning pointer. Callers must not delete it.

---

### Task 7 — Fix the `catalog.h` Header

**File:** [`include/catalog/catalog.h`](file:///Users/jaswanth/Desktop/miniDB/include/catalog/catalog.h)

**What to fix:**
1. Change `#include "schema.h"` and `#include "tuple.h"` → `#include "catalog/schema.h"` and `#include "catalog/tuple.h"` (relative to the `include/` root, consistent with every other header in the project).
2. Change `#include "include/storage/..."` → `#include "storage/..."` (drop the redundant `include/` prefix — CMake adds `include/` to the include path already).
3. Rename `saveToDisk()` → `SaveToDisk()` in the public API.
4. Add a `private:` `void LoadFromDisk();` declaration.
5. Remove `using namespace std;` from a header (pull it into the `.cpp` only).

---

### Task 8 — Add `catalog.cpp` to `CMakeLists.txt`

**File:** [`CMakeLists.txt`](file:///Users/jaswanth/Desktop/miniDB/CMakeLists.txt)

**What to do:**
- Add `src/catalog/catalog.cpp` to `STORAGE_SOURCES` so it is compiled into `minidb`, `integration_test`, and `btree_benchmark`.

```cmake
set(STORAGE_SOURCES
    src/storage/DiskManager.cpp
    src/storage/BufferPool.cpp
    src/storage/page.cpp
    src/catalog/tuple.cpp
    src/catalog/catalog.cpp      # ← add this
    src/storage/rid.cpp
    src/storage/TableHeap.cpp
    src/storage/TableIterator.cpp
)
```

---

### Task 9 — Write a Catalog Unit Test

**File:** `test/catalog_test.cpp` *(new file)*

**Test cases to cover:**

| # | Test | What it validates |
|---|------|-------------------|
| 1 | `CreateTable` then `GetTable` | Returns non-null `TableInfo` with correct name/schema |
| 2 | `GetTable` on unknown name | Returns `nullptr` |
| 3 | Duplicate `CreateTable` | Second call is a no-op, doesn't corrupt the map |
| 4 | **Persistence round-trip** | Create a table, destroy `Catalog`, construct a new `Catalog` from the same `DiskManager`, call `GetTable` — must find the table with the same schema and `first_page_id` |
| 5 | Schema column round-trip | Verify column names, types, and lengths survive serialization/deserialization |
| 6 | Multiple tables | Create 3 tables, restart, verify all 3 are present |

Add the test to `CMakeLists.txt`:
```cmake
add_executable(catalog_test
    test/catalog_test.cpp
    ${STORAGE_SOURCES}
)
target_include_directories(catalog_test PRIVATE include)
add_test(NAME catalog_test COMMAND catalog_test)
```

---

### Task 10 — Update `src/main.cpp` to Use the Catalog

**File:** [`src/main.cpp`](file:///Users/jaswanth/Desktop/miniDB/src/main.cpp)

Replace any ad-hoc table creation with the `Catalog` API:
```cpp
DiskManager dm("minidb.db");
BufferPoolManager bpm(&dm);
Catalog catalog(&bpm);            // loads from disk (or empty on first run)

catalog.CreateTable("users", Schema({
    {"id",   TypeId::INT,     0},
    {"name", TypeId::VARCHAR, 64},
}));

TableInfo* info = catalog.GetTable("users");
// use info->heap to insert/read tuples
```

---

## Verification Plan

### Build
```bash
cd /Users/jaswanth/Desktop/miniDB/build
cmake .. && make -j4
```
All targets must compile without errors.

### Automated Tests
```bash
ctest --output-on-failure
```
- `catalog_test` — all 6 cases pass (especially the persistence round-trip).
- `integration_test` — existing tuple/heap tests still pass.
- `bplus_tree_test` — B-tree unaffected.

### Manual Check
Run `minidb` twice:
1. First run: tables are created, catalog is written to page 0.
2. Second run: catalog is loaded, `GetTable("users")` returns the correct `TableInfo`.

---

## Summary of Files Touched

| File | Action |
|------|--------|
| `include/catalog/catalog.h` | Fix includes, rename method, add `LoadFromDisk` decl |
| `src/catalog/catalog.cpp` | **Create** — full implementation |
| `src/storage/DiskManager.cpp` / `.h` | Reserve page 0 on fresh DB |
| `CMakeLists.txt` | Add `catalog.cpp` to `STORAGE_SOURCES`, add `catalog_test` target |
| `test/catalog_test.cpp` | **Create** — 6 test cases |
| `src/main.cpp` | Wire up `Catalog` instead of raw `TableHeap` |
