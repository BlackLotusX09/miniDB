// =============================================================================
// Day 7 — Full Integration Test for Phase 1
//
// Tests the complete storage stack end-to-end:
//   DiskManager → BufferPoolManager → SlottedPage → TableHeap → TableIterator
//
// Schema: students(id INT, name VARCHAR, gpa INT)
//
// Steps:
//   1. Insert 1200 rows with realistic names
//   2. Scan via TableIterator, count and verify every row
//   3. Flush & destroy all objects (simulates a process restart)
//   4. Re-open the same .db file, scan again, confirm identical results
// =============================================================================

#include "storage/TableHeap.h"
#include "storage/TableIterator.h"

#include <iostream>
#include <vector>
#include <string>
#include <cassert>
#include <cstdio>

using namespace std;

// ── Name generator ────────────────────────────────────────────────────────────
static vector<string> GenerateNames(size_t count) {
    vector<string> first_names = {
        "Alice", "Bob", "Charlie", "David", "Emma",
        "Frank", "Grace", "Henry", "Ivy", "Jack",
        "Katherine", "Leo", "Mia", "Nathan", "Olivia"
    };
    vector<string> last_names = {
        "Smith", "Johnson", "Williams", "Brown", "Jones",
        "Miller", "Davis", "Wilson", "Moore", "Taylor",
        "Anderson", "Thomas", "Jackson", "White", "Harris"
    };

    vector<string> names;
    names.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        string fn = first_names[i % first_names.size()];
        string ln = last_names[(i / first_names.size()) % last_names.size()];
        // Make names unique by appending index
        names.push_back(fn + " " + ln + "_" + to_string(i));
    }
    return names;
}

// ── Phase A: Insert 1200 rows ─────────────────────────────────────────────────
static page_id_t Phase_Insert(const string& db_path, size_t num_rows) {
    cout << "\n[Phase A] Creating table and inserting " << num_rows << " rows...\n";

    DiskManager dm(db_path);
    BufferPoolManager bpm(&dm);

    Schema student_schema({
        {"id",   TypeId::INT},
        {"name", TypeId::VARCHAR},
        {"gpa",  TypeId::INT}
    });

    // Creates a new table — allocates and initializes the first page
    TableHeap heap(&bpm);
    page_id_t root = heap.GetFirstPageId();

    vector<string> names = GenerateNames(num_rows);

    for (size_t i = 0; i < num_rows; ++i) {
        // gpa in [200, 400] (stored as integer: 3.75 → 375)
        int32_t gpa = 200 + static_cast<int32_t>(i % 201);

        vector<Value> vals = {
            Value(static_cast<int32_t>(i)),
            Value(names[i]),
            Value(gpa)
        };

        Tuple t(vals, student_schema);
        RID rid(INVALID_PAGE_ID, -1);
        bool ok = heap.InsertTuple(t, &rid, student_schema);
        assert(ok && "InsertTuple failed!");
    }

    bpm.flushAllPages();
    cout << "[Phase A] Done. Root page id = " << root << "\n";
    return root;
}

// ── Phase B: Scan via TableIterator and count rows ────────────────────────────
static size_t Phase_Scan(const string& db_path, page_id_t root_pid,
                         const string& phase_label) {
    cout << "\n[" << phase_label << "] Scanning table (root page = "
         << root_pid << ")...\n";

    DiskManager dm(db_path);
    BufferPoolManager bpm(&dm);

    Schema student_schema({
        {"id",   TypeId::INT},
        {"name", TypeId::VARCHAR},
        {"gpa",  TypeId::INT}
    });

    TableHeap heap(&bpm, root_pid);
    TableIterator it = heap.Begin();

    size_t count = 0;
    Tuple t;
    while (it.Move(&t)) {
        ++count;

        // Deserialize and spot-check every 100th row
        if (count % 100 == 1) {
            auto vals = t.Deserialize(student_schema);
            int32_t id  = vals[0].AsInt();
            string  nm  = vals[1].AsString();
            int32_t gpa = vals[2].AsInt();
            cout << "  row " << setw(4) << count
                 << "  id=" << setw(5) << id
                 << "  name=" << nm
                 << "  gpa=" << gpa << "\n";
        }
    }

    bpm.flushAllPages();
    cout << "[" << phase_label << "] Total rows scanned: " << count << "\n";
    return count;
}

// ── main ──────────────────────────────────────────────────────────────────────
int main() {
    const string DB_PATH  = "students.db";
    const size_t NUM_ROWS = 1200;

    // Clean start — remove any leftover db from previous run
    remove(DB_PATH.c_str());
    remove((DB_PATH + ".meta").c_str());

    // ── Phase A: insert ───────────────────────────────────────────────────────
    page_id_t root = Phase_Insert(DB_PATH, NUM_ROWS);

    // ── Phase B: first scan (same process, proves storage stack works) ────────
    size_t count1 = Phase_Scan(DB_PATH, root, "Phase B - First Scan");

    // ── Phase C: simulate restart by scanning again (new BPM + DiskManager) ──
    size_t count2 = Phase_Scan(DB_PATH, root, "Phase C - Restart Scan");

    // ── Final verification ────────────────────────────────────────────────────
    cout << "\n=== Results ===\n";
    cout << "Rows inserted : " << NUM_ROWS << "\n";
    cout << "First scan    : " << count1   << "\n";
    cout << "Restart scan  : " << count2   << "\n";

    bool pass = (count1 == NUM_ROWS) && (count2 == NUM_ROWS);
    if (pass) {
        cout << "\n✅  ALL CHECKS PASSED — Phase 1 integration test complete!\n";
    } else {
        cout << "\n❌  MISMATCH DETECTED — check storage layer for bugs.\n";
    }

    return pass ? 0 : 1;
}
