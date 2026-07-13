# miniDB

A from-scratch relational database engine written in C++17, built one layer at a time. This project covers the full storage stack — disk I/O, buffer pool management, slotted pages, heap files, and iterators — following a structured day-by-day plan.

---

## Phase 1 — Storage Engine

### Architecture

```
SQL Query (future)
      │
      ▼
 TableIterator         ← sequential scan over a heap file
      │
      ▼
  TableHeap            ← manages a linked list of slotted pages
      │
      ▼
BufferPoolManager      ← in-memory cache of pages with LRU eviction
      │
      ▼
  DiskManager          ← reads/writes fixed-size pages to a .db file
```

---

### Page Format

Each page is exactly **4 096 bytes** and uses a **slotted-page** layout:

```
┌────────────────────────────────────────────────────────────┐
│  PageHeader  (16 bytes)                                    │
│    page_id        : uint32  — this page's ID               │
│    num_slots      : uint16  — number of slot entries       │
│    free_space_ptr : uint16  — offset of next free byte     │
│    next_page_id   : int32   — next page in chain (-1=none) │
├────────────────────────────────────────────────────────────┤
│  Slot Array  [num_slots × 4 bytes]  ──► grows downward     │
│    SlotEntry { offset: uint16, length: uint16 }            │
├────────────────────────────────────────────────────────────┤
│  Free Space                                                │
├────────────────────────────────────────────────────────────┤
│  Record Data  ◄── grows upward from the end of the page   │
└────────────────────────────────────────────────────────────┘
```

Records are inserted from the **end** of the page growing toward the front, while the slot array grows forward. A slot is "tombstoned" (deleted) by setting its `length` field to 0 — the iterator skips these automatically.

---

### Buffer Pool Design

`BufferPoolManager` maintains a fixed pool of **32 frames** in memory. Each frame holds one 4 KB page, a dirty flag, and a pin count.

| Concept | Implementation |
|---|---|
| **Eviction policy** | LRU (Least Recently Used) via a doubly-linked list + hash map |
| **Pinning** | A page is "pinned" while in use; only unpinned pages are evictable |
| **Dirty tracking** | Pages marked dirty are written back before the frame is reused |
| **Free frames** | A FIFO queue of frame IDs; exhausted frames go through LRU eviction |

**LRU Replacer**: Frames enter the LRU list when their pin count drops to 0 (`unpin`). The most-recently-used unpinned frame is at the head; the least-recently-used is at the tail. On eviction, the tail is chosen and removed.

---

### DiskManager & Persistence

`DiskManager` maps page IDs to fixed-size byte regions on disk:

```
offset = page_id × PAGE_SIZE   (4 096 bytes per page)
```

A companion `.meta` sidecar file stores `next_page_id` so that page allocation survives process restarts. Without this, reopening an existing `.db` file would re-allocate from page 0, corrupting existing data.

---

### Day 7 — Full Integration Test

The test in [`test/integration_test.cpp`](test/integration_test.cpp) exercises the complete stack end-to-end:

1. **Phase A** — Creates a `students(id INT, name VARCHAR, gpa INT)` table and inserts **1 200 rows** with realistic generated names.
2. **Phase B** — Scans the table via `TableIterator`, counts all rows, and spot-checks deserialized values.
3. **Phase C** — Destroys all objects (simulating a process restart), reopens the same `.db` file with a fresh `DiskManager` + `BufferPoolManager`, scans again, and confirms the row count is identical.

**Result**: All 1 200 rows are found in both scans. ✅

#### Build & Run

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target integration_test
cd build && ./integration_test
```

---

### Key Bugs Fixed During Phase 1

| Bug | Root Cause | Fix |
|---|---|---|
| **Infinite iterator loop** | `SlottedPage::Init()` did `memset(0)` but never set `next_page_id = -1`. Since 0 is a valid page ID, the iterator cycled back to the first page indefinitely. | `Init()` now explicitly sets `next_page_id = INVALID_PAGE_ID`. |
| **Page 0 never flushed** | `flushAllPages()` had the guard `page_id != 0`, incorrectly treating page 0 as an "empty frame" sentinel. | Guard changed to `page_table_.count(pid)` to check actual occupancy. |
| **Restart loses page count** | `DiskManager` initialized `next_page_id = 0` every time, causing page ID collisions after restart. | Persist `next_page_id` in a `.meta` sidecar file; load it on open. |
| **Circular include** | `TableIterator.h` included `TableHeap.h` which included `TableIterator.h`. | Removed the unnecessary `TableHeap.h` include from `TableIterator.h`. |

---

## Roadmap

- **Phase 2** — B-Tree index, catalog, SQL parser (SELECT / INSERT / CREATE TABLE)
- **Phase 3** — Query execution engine, joins, aggregates
- **Phase 4** — Transactions, write-ahead logging, recovery
