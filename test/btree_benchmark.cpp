// =============================================================================
// Day 17 — Benchmarks: the numbers that go on your resume
//
// Benchmarks:
//   1. Insert throughput: keys/second for 100K insertions
//   2. Point lookup:      B-tree vs sequential scan on 100K rows
//   3. Range scan:        B-tree 10% range vs full table filter
//   4. Tree height:       height at 100, 1K, 10K, 100K keys (O(log N) proof)
//
// Each timed section runs 3× and takes the median.
// =============================================================================

#include <iostream>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <chrono>
#include <string>
#include <cassert>
#include <cstdio>
#include <cmath>

#include "storage/DiskManager.h"
#include "storage/BufferPool.h"
#include "storage/b_tree.h"
#include "storage/TableHeap.h"
#include "storage/TableIterator.h"
#include "catalog/schema.h"
#include "catalog/tuple.h"
#include "catalog/value.h"
#include "storage/rid.h"

using namespace std;
using Clock = chrono::high_resolution_clock;

static double elapsed_ms(Clock::time_point t0, Clock::time_point t1) {
    return chrono::duration<double, milli>(t1 - t0).count();
}

template<typename Fn>
static double median3(Fn fn) {
    double s[3];
    for (int i = 0; i < 3; i++) {
        auto t0 = Clock::now(); fn(); auto t1 = Clock::now();
        s[i] = elapsed_ms(t0, t1);
    }
    sort(s, s + 3);
    return s[1];
}

// ── Tree factory ──────────────────────────────────────────────────────────────
static BPlusTree* MakeTree(const string& db, DiskManager*& dm,
                           BufferPoolManager*& bpm, page_id_t& hid) {
    remove(db.c_str());
    dm  = new DiskManager(db);
    bpm = new BufferPoolManager(dm);
    Page* h = bpm->NewPage(&hid);
    assert(h);
    bpm->UnpinPage(hid, false);
    return new BPlusTree(bpm, hid);
}
static void DestroyTree(const string& db, BPlusTree* t,
                        DiskManager* dm, BufferPoolManager* bpm) {
    delete t; delete bpm; delete dm;
    remove(db.c_str());
}

static void section(const char* title) {
    cout << "\n" << string(60,'=') << "\n  " << title << "\n" << string(60,'=') << "\n";
}

// =============================================================================
int main() {
    cout << "\n╔══════════════════════════════════════════════════════════╗\n";
    cout <<   "║        miniDB — Day 17 Benchmark Suite                  ║\n";
    cout <<   "╚══════════════════════════════════════════════════════════╝\n";

    const int N = 100000;

    // ──────────────────────────────────────────────────────────────────────────
    // BENCHMARK 1: Insert throughput
    // ──────────────────────────────────────────────────────────────────────────
    section("Benchmark 1 — Insert throughput (100K keys)");
    double ins_ms;
    {
        DiskManager* dm; BufferPoolManager* bpm; page_id_t hid;
        BPlusTree* tree = MakeTree("bench_insert.db", dm, bpm, hid);
        auto t0 = Clock::now();
        for (int i = 1; i <= N; i++) {
            RID rid(static_cast<page_id_t>(i >> 4), static_cast<slot_id_t>(i & 0xF));
            tree->insert(i, rid);
        }
        auto t1 = Clock::now();
        ins_ms = elapsed_ms(t0, t1);
        DestroyTree("bench_insert.db", tree, dm, bpm);
    }
    double ins_kps = (N / (ins_ms / 1000.0)) / 1000.0;
    cout << "  100K inserts in " << fixed << setprecision(1) << ins_ms << " ms\n";
    cout << "  Throughput: " << fixed << setprecision(0) << ins_kps << " K keys/sec\n";

    // ──────────────────────────────────────────────────────────────────────────
    // BENCHMARK 2: Point lookup — build shared heap+tree
    // ──────────────────────────────────────────────────────────────────────────
    section("Benchmark 2 — Point lookup: B-tree vs sequential scan (100K rows)");
    double btree_lookup_ms, scan_lookup_ms;
    {
        DiskManager* dm; BufferPoolManager* bpm; page_id_t hid;
        BPlusTree* tree = MakeTree("bench_lookup.db", dm, bpm, hid);
        Schema schema({ {"id", TypeId::INT} });
        TableHeap heap(bpm);

        for (int i = 1; i <= N; i++) {
            Tuple t({ Value(i) }, schema);
            RID rid;
            heap.InsertTuple(t, &rid, schema);
            tree->insert(i, rid);
        }

        int target = N / 2;

        btree_lookup_ms = median3([&](){
            vector<RID> r;
            bool f = tree->Search(target, &r);
            assert(f); (void)f;
        });

        scan_lookup_ms = median3([&](){
            TableIterator it = heap.Begin();
            Tuple t; int fid = -1;
            while (it.Move(&t)) {
                auto v = t.Deserialize(schema);
                if (v[0].AsInt() == target) { fid = v[0].AsInt(); break; }
            }
            assert(fid == target); (void)fid;
        });

        DestroyTree("bench_lookup.db", tree, dm, bpm);
    }
    double lookup_speedup = scan_lookup_ms / btree_lookup_ms;
    cout << "  B-tree point lookup : " << fixed << setprecision(3) << btree_lookup_ms << " ms  (median 3×)\n";
    cout << "  Sequential scan     : " << fixed << setprecision(2) << scan_lookup_ms  << " ms  (median 3×)\n";
    cout << "  Speedup             : " << fixed << setprecision(0) << lookup_speedup << "×\n";

    // ──────────────────────────────────────────────────────────────────────────
    // BENCHMARK 3: Range scan — 10% of rows
    // ──────────────────────────────────────────────────────────────────────────
    section("Benchmark 3 — Range scan: B-tree vs full scan (10% of 100K rows)");
    double btree_range_ms, scan_range_ms;
    size_t range_count = 0;
    {
        int lo = N * 45 / 100, hi = N * 55 / 100;

        DiskManager* dm; BufferPoolManager* bpm; page_id_t hid;
        BPlusTree* tree = MakeTree("bench_range.db", dm, bpm, hid);
        Schema schema({ {"id", TypeId::INT} });
        TableHeap heap(bpm);

        for (int i = 1; i <= N; i++) {
            Tuple t({ Value(i) }, schema);
            RID rid;
            heap.InsertTuple(t, &rid, schema);
            tree->insert(i, rid);
        }

        btree_range_ms = median3([&](){
            auto rids = tree->RangeScan(lo, hi);
            range_count = rids.size();
        });

        scan_range_ms = median3([&](){
            TableIterator it = heap.Begin();
            Tuple t; size_t cnt = 0;
            while (it.Move(&t)) {
                auto v = t.Deserialize(schema);
                int id = v[0].AsInt();
                if (id >= lo && id <= hi) cnt++;
            }
            (void)cnt;
        });

        cout << "  Range [" << lo << ", " << hi << "] — " << range_count << " rows\n";
        cout << "  B-tree range scan   : " << fixed << setprecision(2) << btree_range_ms << " ms  (median 3×)\n";
        cout << "  Sequential scan     : " << fixed << setprecision(2) << scan_range_ms  << " ms  (median 3×)\n";
        double rs_speedup = scan_range_ms / btree_range_ms;
        cout << "  Speedup             : " << fixed << setprecision(0) << rs_speedup << "×\n";

        DestroyTree("bench_range.db", tree, dm, bpm);
    }

    // ──────────────────────────────────────────────────────────────────────────
    // BENCHMARK 4: Tree height at 100, 1K, 10K, 100K
    // ──────────────────────────────────────────────────────────────────────────
    section("Benchmark 4 — Tree height growth (O(log N) proof)");
    cout << "  " << left << setw(10) << "Keys" << setw(10) << "Height" << "log2(N)\n";
    cout << "  " << string(30, '-') << "\n";
    for (int n : {100, 1000, 10000, 100000}) {
        DiskManager* dm; BufferPoolManager* bpm; page_id_t hid;
        BPlusTree* tree = MakeTree("bench_height.db", dm, bpm, hid);
        for (int i = 1; i <= n; i++) {
            RID rid(0, static_cast<slot_id_t>(i & 0xFFFF));
            tree->insert(i, rid);
        }
        int h = tree->GetTreeHeight();
        cout << "  " << left << setw(10) << n << setw(10) << h
             << fixed << setprecision(1) << log2((double)n) << "\n";
        DestroyTree("bench_height.db", tree, dm, bpm);
    }

    // ──────────────────────────────────────────────────────────────────────────
    // SUMMARY
    // ──────────────────────────────────────────────────────────────────────────
    double range_speedup = scan_range_ms / btree_range_ms;
    cout << "\n" << string(60,'=') << "\n";
    cout << "  SUMMARY\n" << string(60,'=') << "\n";
    cout << "  Insert throughput : " << fixed << setprecision(0) << ins_kps << " K keys/sec\n";
    cout << "  Point lookup      : " << fixed << setprecision(3) << btree_lookup_ms
         << " ms B-tree  vs  " << fixed << setprecision(2) << scan_lookup_ms
         << " ms scan  (" << fixed << setprecision(0) << lookup_speedup << "× speedup)\n";
    cout << "  Range scan (10%)  : " << fixed << setprecision(2) << btree_range_ms
         << " ms B-tree  vs  " << fixed << setprecision(2) << scan_range_ms
         << " ms scan  (" << fixed << setprecision(0) << range_speedup << "× speedup)\n";
    cout << string(60,'=') << "\n\n";

    return 0;
}
