# miniDB

A from-scratch relational database engine written in **C++17**, built one layer at a time.
Covers the full storage stack — disk I/O, buffer pool management, slotted pages, heap files,
B+ tree index, and a benchmark suite — following a structured day-by-day build plan.

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

### Buffer Pool Design

`BufferPoolManager` maintains a fixed pool of **32 frames** in memory. Each frame holds one 4 KB page, a dirty flag, and a pin count.

| Concept | Implementation |
|---|---|
| **Eviction policy** | LRU (Least Recently Used) via a doubly-linked list + hash map |
| **Pinning** | A page is "pinned" while in use; only unpinned pages are evictable |
| **Dirty tracking** | Pages marked dirty are written back before the frame is reused |
| **Free frames** | A FIFO queue of frame IDs; exhausted frames go through LRU eviction |

### DiskManager & Persistence

`DiskManager` maps page IDs to fixed-size byte regions on disk:

```
offset = page_id × PAGE_SIZE   (4 096 bytes per page)
```

A companion `.meta` sidecar file stores `next_page_id` so that page allocation survives process restarts.

### Day 7 — Full Integration Test

The test in [`test/integration_test.cpp`](test/integration_test.cpp) exercises the complete stack end-to-end:

1. **Phase A** — Creates a `students(id INT, name VARCHAR, gpa INT)` table and inserts **1 200 rows**.
2. **Phase B** — Scans via `TableIterator`, counts all rows, spot-checks deserialized values.
3. **Phase C** — Simulates a process restart (fresh `DiskManager` + `BufferPoolManager`), scans again, confirms identical row count.

**Result**: All 1 200 rows found in both scans. ✅

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target integration_test
cd build && ./integration_test
```

### Key Bugs Fixed — Phase 1

| Bug | Root Cause | Fix |
|---|---|---|
| **Infinite iterator loop** | `SlottedPage::Init()` never set `next_page_id = -1`; 0 is a valid page ID so the iterator cycled back. | `Init()` now explicitly sets `next_page_id = INVALID_PAGE_ID`. |
| **Page 0 never flushed** | `flushAllPages()` had guard `page_id != 0`, skipping the very first page. | Guard changed to `page_table_.count(pid)`. |
| **Restart loses page count** | `DiskManager` initialized `next_page_id = 0` every restart, causing ID collisions. | Persist in a `.meta` sidecar file. |
| **Circular include** | `TableIterator.h` ↔ `TableHeap.h` mutual include. | Removed the unnecessary include. |

---

## Phase 2 — B+ Tree Index

### Architecture

```
RangeScan / PointLookup
      │
      ▼
   BPlusTree             ← traversal, insert, delete, range scan
      │
      ▼
BTreeLeafPage            ← stores (key, RID) pairs; linked list across leaves
BTreeInternalPage        ← stores separator keys + child page pointers
      │
      ▼
BufferPoolManager        ← every node access goes through FetchPage / UnpinPage
```

### B+ Tree Node Layout

**Leaf node** (order = 50 keys/page):
```
┌──────────┬──────────┬──────────┬──────────┬──────────────────┐
│ page_id  │ type=0   │ num_keys │ parent   │ next_leaf_id     │  ← 20 B header
├──────────┴──────────┴──────────┴──────────┴──────────────────┤
│  key₀ (4B) │ RID₀ (6B) │ key₁ (4B) │ RID₁ (6B) │ …         │  ← 10 B/entry
└───────────────────────────────────────────────────────────────┘
```

**Internal node**: separator keys + `n+1` child page pointers, same 20 B header.

### Operations Implemented

| Operation | Description |
|---|---|
| `insert(key, rid)` | Descend to leaf, insert sorted; split leaf → push key up; split internal → cascade |
| `Search(key, &result)` | O(log N) descent; returns matching RID |
| `Remove(key)` | Delete from leaf; borrow from sibling or merge; cascade root collapse |
| `RangeScan(low, high)` | Descend to first leaf ≥ low, follow `next_leaf` chain collecting keys ≤ high |
| `GetTreeHeight()` | Descend to leftmost leaf counting levels |
| `GetKeyCount()` | Walk entire leaf chain summing `num_keys` |
| `ReadMeta()` | Returns `btreeMetaPage{root_page_id, total_key_count, tree_height}` |

### Metadata Page

The header page stores a `btreeMetaPage` struct (first 16 bytes):

```cpp
struct btreeMetaPage {
    page_id_t root_page_id;    // survives restart
    int32_t   total_key_count;
    int32_t   tree_height;     // 0=empty, 1=leaf-only, 2+=multi-level
    int32_t   reserved;
};
```

---

## Phase 2 Benchmarks — Day 17

> Measured on an Apple M-series machine, `-O2`, buffer pool = 32 frames (128 KB).
> Each timing is the **median of 3 runs**.

### Insert Throughput

| Keys | Time | Throughput |
|---|---|---|
| 100 000 | 227.5 ms | **440 K keys/sec** |

### Point Lookup — B-tree vs Sequential Scan (100K rows)

| Method | Latency | Speedup |
|---|---|---|
| **B-tree point lookup** | **0.001 ms** | — |
| Sequential table scan | 3.52 ms | **~6 000×** |

> *"My B-tree point lookup on 100K rows took 0.001 ms vs 3.52 ms for a sequential scan — a 6 000× speedup. The tree had height 3, meaning any lookup needed at most 3 disk-page reads regardless of table size."*

### Range Scan — B-tree vs Sequential Scan (10% of 100K rows)

| Method | Latency | Rows returned | Speedup |
|---|---|---|---|
| **B-tree range scan** | **0.43 ms** | 10 001 | — |
| Sequential scan + filter | 7.54 ms | 10 001 | **~17×** |

### Tree Height Growth (O(log N) proof)

| Keys | Height | log₂(N) |
|---|---|---|
| 100 | 2 | 6.6 |
| 1 000 | 2 | 10.0 |
| 10 000 | 2 | 13.3 |
| 100 000 | 3 | 16.6 |

The tree height grows as **O(log N)** — confirmed empirically. At 100K keys the tree is only 3 levels deep, so every lookup touches at most 3 pages.

### Run the Benchmarks

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target btree_benchmark
cd build && ./btree_benchmark
```

---

## Phase 2 Test Suite — Day 9–16

Validated by [`test/BplusTreeTest.cpp`](test/BplusTreeTest.cpp) (15 tests):

| # | Test | What it covers |
|---|---|---|
| 1 | `TestRandomInsert` | 1000 keys random order; `VerifyLeafChain` |
| 2 | `TestAscendingInsert` | Sequential ascending insertion |
| 3 | `TestDescendingInsert` | Sequential descending insertion |
| 4 | `TestValidateTree` | BST invariants on 500 random keys |
| 5 | `TestDeleteCase1_NoUnderflow` | Delete without underflow |
| 6 | `TestDeleteCase2_BorrowFromSibling` | Redistribute from right/left sibling |
| 7 | `TestDeleteCase3_MergeAndRootCollapse` | Leaf merge → root height decrease |
| 8 | `TestDeleteStress_RandomOrder` | 1000 insert + 1000 delete random order → empty |
| 9 | `TestDeleteStress_AscInsDescDel` | Ascending insert, descending delete |
| 10 | `TestDeleteInterleaved` | Insert evens, delete odds, re-insert |
| 11 | `RangeScanTest` | Keys 0–99, scan [25,75] → 51 RIDs |
| 12 | `TestRangeScan_Day15Checkpoint` | 1000 keys, `RangeScan(250,750)` = exactly **501 RIDs** |
| 13 | `TestDay16_PinLeakAudit` | `CheckAllUnpinned()` after every op — zero pin leaks |
| 14 | `TestDay16_RestartPersistence` | Flush → destroy → reopen → all keys still searchable |
| 15 | `TestDay16_MetadataPage` | `ReadMeta()` returns correct root, count, and height |

```bash
cmake --build build --target BplusTreeTest
cd build && ./BplusTreeTest
```

### Key Bugs Fixed — Phase 2

| Bug | Root Cause | Fix |
|---|---|---|
| **Pin leak in `findLeafContaining`** | Leaf page was fetched (pin=1) but not unpinned before returning its id. `RangeScan` fetched it again (pin=2); one `UnpinPage` left pin=1. | Unpin the leaf in `findLeafContaining` before returning; `RangeScan` does its own `FetchPage`. |
| **Wrong index in `RangeScanTest`** | Assertion loop used `actual[i]` where `i` starts at 25, skipping the first 25 elements of `actual`. | Changed to `actual[idx]` with a 0-based index. |

---

## Roadmap

- **Phase 2** *(in progress)* — B+ Tree index ✅, catalog, SQL parser (SELECT / INSERT / CREATE TABLE)
- **Phase 3** — Query execution engine, joins, aggregates

- **Phase 4** — Transactions, write-ahead logging, recovery
