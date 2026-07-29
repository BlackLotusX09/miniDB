
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <random>
#include <cassert>
#include <climits>

// Your database engine includes
#include "storage/DiskManager.h"
#include "storage/BufferPool.h"
#include "storage/b+tree.h"
#include "storage/rid.h"

// Helper: read the root page_id from the header page
static page_id_t GetRootId(BufferPoolManager* bpm, page_id_t header_id) {
    Page* hp = bpm->FetchPage(header_id);
    assert(hp);
    page_id_t root;
    std::memcpy(&root, hp->GetData(), sizeof(page_id_t));
    bpm->UnpinPage(header_id, false);
    return root;
}

// Helper to set up a clean database environment for each test run
BPlusTree* SetupTree(const std::string& fileName, DiskManager*& dm, BufferPoolManager*& bpm) {
    std::remove(fileName.c_str());
    dm = new DiskManager(fileName);
    bpm = new BufferPoolManager(dm);

    page_id_t header_id;
    Page* raw_header_frame = bpm->NewPage(&header_id);
    assert(raw_header_frame != nullptr);
    bpm->UnpinPage(header_id, false);

    return new BPlusTree(bpm, header_id);
}

// Helper to clean up memory after a test finishes
void TeardownTree(const std::string& fileName, BPlusTree* tree, DiskManager* dm, BufferPoolManager* bpm) {
    delete tree;
    delete bpm;
    delete dm;
    std::remove(fileName.c_str());
}

// ============================================================================
// 1. RANDOM SHUFFLE INSERTION TEST
// ============================================================================
void TestRandomInsert() {
    std::cout << "[Running] TestRandomInsert (1000 keys, random order)...\n";
    std::string fileName = "test_random.db";
    DiskManager* dm = nullptr;
    BufferPoolManager* bpm = nullptr;
    BPlusTree* tree = SetupTree(fileName, dm, bpm);

    // Generate keys 1 to 1000
    std::vector<int> key_container;
    for (int i = 1; i <= 1000; i++) {
        key_container.push_back(i);
    }

    // Shuffle keys randomly
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(key_container.begin(), key_container.end(), g);

    // Insert keys
    for (int key : key_container) {
        RID rid(static_cast<page_id_t>(0), static_cast<slot_id_t>(key));
        tree->insert(key, rid);
    }

    // Verify all keys can be searched and found
    for (int i = 1; i <= 1000; i++) {
        std::vector<RID> result;
        bool found = tree->Search(i, &result);
        assert(found && "Key not found during random stress search!");
        assert(result.size() == 1);
        assert(result[0].slot_id == static_cast<slot_id_t>(i));
    }

    // Structural validation
    page_id_t root = GetRootId(bpm, 1 /*header_id is always page 1*/);
    // Note: header is allocated as the first page — find it via the tree
    // ValidateTree uses bpm_, which we can't access directly, so we call on tree
    // Build expected sorted key list
    std::vector<int32_t> expected;
    for (int i = 1; i <= 1000; i++) expected.push_back(i);

    tree->VerifyLeafChain(expected);
    std::cout << "  [+] VerifyLeafChain passed.\n";

    TeardownTree(fileName, tree, dm, bpm);
    std::cout << "[Passed] TestRandomInsert successfully completed.\n\n";
}

// ============================================================================
// 2. ASCENDING SEQUENTIAL INSERTION TEST
// ============================================================================
void TestAscendingInsert() {
    std::cout << "[Running] TestAscendingInsert (1000 keys, ascending)...\n";
    std::string fileName = "test_ascending.db";
    DiskManager* dm = nullptr;
    BufferPoolManager* bpm = nullptr;
    BPlusTree* tree = SetupTree(fileName, dm, bpm);

    // Insert keys sequentially (1 to 1000)
    for (int i = 1; i <= 1000; i++) {
        RID rid(static_cast<page_id_t>(0), static_cast<slot_id_t>(i));
        tree->insert(i, rid);
    }

    // Verify search
    for (int i = 1; i <= 1000; i++) {
        std::vector<RID> result;
        bool found = tree->Search(i, &result);
        assert(found && "Key not found during ascending sequential search!");
        assert(result[0].slot_id == static_cast<slot_id_t>(i));
    }

    // Structural validation
    std::vector<int32_t> expected;
    for (int i = 1; i <= 1000; i++) expected.push_back(i);
    tree->VerifyLeafChain(expected);
    std::cout << "  [+] VerifyLeafChain passed.\n";

    TeardownTree(fileName, tree, dm, bpm);
    std::cout << "[Passed] TestAscendingInsert successfully completed.\n\n";
}

// ============================================================================
// 3. DESCENDING SEQUENTIAL INSERTION TEST
// ============================================================================
void TestDescendingInsert() {
    std::cout << "[Running] TestDescendingInsert (1000 keys, descending)...\n";
    std::string fileName = "test_descending.db";
    DiskManager* dm = nullptr;
    BufferPoolManager* bpm = nullptr;
    BPlusTree* tree = SetupTree(fileName, dm, bpm);

    // Insert keys in reverse order (1000 down to 1)
    for (int i = 1000; i >= 1; i--) {
        RID rid(static_cast<page_id_t>(0), static_cast<slot_id_t>(i));
        tree->insert(i, rid);
    }

    // Verify search
    for (int i = 1; i <= 1000; i++) {
        std::vector<RID> result;
        bool found = tree->Search(i, &result);
        assert(found && "Key not found during descending sequential search!");
        assert(result[0].slot_id == static_cast<slot_id_t>(i));
    }

    // Structural validation
    std::vector<int32_t> expected;
    for (int i = 1; i <= 1000; i++) expected.push_back(i);
    tree->VerifyLeafChain(expected);
    std::cout << "  [+] VerifyLeafChain passed.\n";

    TeardownTree(fileName, tree, dm, bpm);
    std::cout << "[Passed] TestDescendingInsert successfully completed.\n\n";
}

// ============================================================================
// 4. TREE STRUCTURE VALIDATOR TEST (ValidateTree)
// ============================================================================
void TestValidateTree() {
    std::cout << "[Running] TestValidateTree (BST invariant check, random 500 keys)...\n";
    std::string fileName = "test_validate.db";
    DiskManager* dm = nullptr;
    BufferPoolManager* bpm = nullptr;
    BPlusTree* tree = SetupTree(fileName, dm, bpm);

    std::vector<int> keys;
    for (int i = 1; i <= 500; i++) keys.push_back(i);
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(keys.begin(), keys.end(), g);

    for (int key : keys) {
        RID rid(0, static_cast<slot_id_t>(key));
        tree->insert(key, rid);
    }

    // Fetch root and run full recursive validator
    Page* hp = bpm->FetchPage(1);
    page_id_t root;
    std::memcpy(&root, hp->GetData(), sizeof(page_id_t));
    bpm->UnpinPage(1, false);

    tree->ValidateTree(root, INT32_MIN, INT32_MAX);
    std::cout << "  [+] ValidateTree passed — all BST invariants hold.\n";

    TeardownTree(fileName, tree, dm, bpm);
    std::cout << "[Passed] TestValidateTree successfully completed.\n\n";
}

// ============================================================================
// 5. DELETE — CASE 1: enough keys after deletion (no underflow)
// ============================================================================
void TestDeleteCase1_NoUnderflow() {
    std::cout << "[Running] TestDeleteCase1_NoUnderflow...\n";
    std::string fileName = "test_del_case1.db";
    DiskManager* dm = nullptr;
    BufferPoolManager* bpm = nullptr;
    BPlusTree* tree = SetupTree(fileName, dm, bpm);

    // Insert enough keys to fill at least 2 leaves but keep it simple (leaf max = 50)
    // 20 keys → single leaf (20 >= minSize=26? No, max=50 minSize=25+1=26... wait,
    // GetMinSize = (50+1)/2 = 25. So 25 keys exactly on the boundary.)
    // Insert 40 keys so the leaf is comfortably above minimum after a few deletions.
    for (int i = 1; i <= 40; i++) {
        RID rid(0, static_cast<slot_id_t>(i));
        tree->insert(i, rid);
    }

    // Delete 5 keys — leaf should still have 35 keys (well above min=25 for a single leaf)
    for (int i = 1; i <= 5; i++) {
        bool ok = tree->Remove(i);
        assert(ok && "Case1: Remove returned false");
    }

    // Verify deleted keys are gone
    for (int i = 1; i <= 5; i++) {
        std::vector<RID> r;
        assert(!tree->Search(i, &r) && "Case1: deleted key still found");
    }
    // Verify remaining keys still present
    for (int i = 6; i <= 40; i++) {
        std::vector<RID> r;
        assert(tree->Search(i, &r) && "Case1: remaining key missing");
    }

    // Structural check
    std::vector<int32_t> expected;
    for (int i = 6; i <= 40; i++) expected.push_back(i);
    tree->VerifyLeafChain(expected);

    TeardownTree(fileName, tree, dm, bpm);
    std::cout << "[Passed] TestDeleteCase1_NoUnderflow\n\n";
}

// ============================================================================
// 6. DELETE — CASE 2: borrow from sibling (redistribute)
// ============================================================================
void TestDeleteCase2_BorrowFromSibling() {
    std::cout << "[Running] TestDeleteCase2_BorrowFromSibling...\n";
    std::string fileName = "test_del_case2.db";
    DiskManager* dm = nullptr;
    BufferPoolManager* bpm = nullptr;
    BPlusTree* tree = SetupTree(fileName, dm, bpm);

    // Insert 100 keys to create several leaves — enough for redistribute to trigger
    for (int i = 1; i <= 100; i++) {
        RID rid(0, static_cast<slot_id_t>(i));
        tree->insert(i, rid);
    }

    // Delete keys from the leftmost half until borrow-from-right fires
    // (delete ~15 keys from keys 1..20 — that leaf will fall to ~10 keys, below min=25,
    //  forcing it to borrow from its right sibling which has 25+ keys)
    std::vector<int32_t> deleted;
    for (int i = 1; i <= 15; i++) {
        bool ok = tree->Remove(i);
        assert(ok && "Case2: Remove returned false");
        deleted.push_back(i);
    }

    // Verify correctness
    for (int d : deleted) {
        std::vector<RID> r;
        assert(!tree->Search(d, &r) && "Case2: deleted key still present");
    }
    for (int i = 16; i <= 100; i++) {
        std::vector<RID> r;
        assert(tree->Search(i, &r) && "Case2: remaining key missing");
    }

    std::vector<int32_t> expected;
    for (int i = 16; i <= 100; i++) expected.push_back(i);
    tree->VerifyLeafChain(expected);

    // Root BST invariants
    page_id_t root = GetRootId(bpm, 1);
    tree->ValidateTree(root, INT32_MIN, INT32_MAX);

    TeardownTree(fileName, tree, dm, bpm);
    std::cout << "[Passed] TestDeleteCase2_BorrowFromSibling\n\n";
}

// ============================================================================
// 7. DELETE — CASE 3: merge with sibling (cascades to root shrink)
// ============================================================================
void TestDeleteCase3_MergeAndRootCollapse() {
    std::cout << "[Running] TestDeleteCase3_MergeAndRootCollapse...\n";
    std::string fileName = "test_del_case3.db";
    DiskManager* dm = nullptr;
    BufferPoolManager* bpm = nullptr;
    BPlusTree* tree = SetupTree(fileName, dm, bpm);

    // 3-key tree (order-3 example from the spec):
    // Insert keys such that after a deletion the leaf merges and root collapses.
    // With leaf MAX_KEYS=50, minSize=25.  To force a merge we need a leaf at minSize
    // then delete from it.  Insert just enough keys to create 2 leaves, then delete
    // until a merge fires.

    // Fill exactly 2 leaves: insert 51 keys (splits into [0..24] and [25..50])
    for (int i = 1; i <= 51; i++) {
        RID rid(0, static_cast<slot_id_t>(i));
        tree->insert(i, rid);
    }

    // Left leaf has 25 keys (minSize), delete 1 to force underflow + merge
    // First key in left leaf is 1.
    bool ok = tree->Remove(1);
    assert(ok && "Case3: Remove(1) returned false");

    // After merge the tree should have a single leaf with 50 keys (2..51)
    // and the root should now be that leaf (height decreased).
    for (int i = 2; i <= 51; i++) {
        std::vector<RID> r;
        assert(tree->Search(i, &r) && "Case3: key missing after merge");
    }
    std::vector<RID> r1;
    assert(!tree->Search(1, &r1) && "Case3: deleted key still visible");

    std::vector<int32_t> expected;
    for (int i = 2; i <= 51; i++) expected.push_back(i);
    tree->VerifyLeafChain(expected);

    page_id_t root = GetRootId(bpm, 1);
    tree->ValidateTree(root, INT32_MIN, INT32_MAX);

    TeardownTree(fileName, tree, dm, bpm);
    std::cout << "[Passed] TestDeleteCase3_MergeAndRootCollapse\n\n";
}

// ============================================================================
// 8. DELETE — STRESS: insert 1000, delete all in random order, verify empty
// ============================================================================
void TestDeleteStress_RandomOrder() {
    std::cout << "[Running] TestDeleteStress_RandomOrder (1000 inserts, 1000 deletes)...\n";
    std::string fileName = "test_del_stress.db";
    DiskManager* dm = nullptr;
    BufferPoolManager* bpm = nullptr;
    BPlusTree* tree = SetupTree(fileName, dm, bpm);

    const int N = 1000;
    std::vector<int> keys;
    for (int i = 1; i <= N; i++) keys.push_back(i);

    // Insert all keys
    std::mt19937 rng(42);
    std::vector<int> ins_order = keys;
    std::shuffle(ins_order.begin(), ins_order.end(), rng);
    for (int k : ins_order) {
        RID rid(0, static_cast<slot_id_t>(k));
        tree->insert(k, rid);
    }

    // Verify all inserted
    for (int i = 1; i <= N; i++) {
        std::vector<RID> r;
        assert(tree->Search(i, &r) && "Stress: key missing before delete");
    }

    // Delete all in a different random order
    std::vector<int> del_order = keys;
    std::shuffle(del_order.begin(), del_order.end(), rng);

    std::vector<bool> deleted(N + 1, false);
    for (int k : del_order) {
        bool ok = tree->Remove(k);
        assert(ok && "Stress: Remove returned false for existing key");
        deleted[k] = true;

        // After each delete, the deleted key must not be found
        std::vector<RID> r;
        assert(!tree->Search(k, &r) && "Stress: deleted key still found");
    }

    // Tree must be empty now
    tree->VerifyLeafChain({});

    TeardownTree(fileName, tree, dm, bpm);
    std::cout << "[Passed] TestDeleteStress_RandomOrder\n\n";
}

// ============================================================================
// 9. DELETE — STRESS: ascending insert, descending delete
// ============================================================================
void TestDeleteStress_AscInsDescDel() {
    std::cout << "[Running] TestDeleteStress_AscInsDescDel (500 keys)...\n";
    std::string fileName = "test_del_ascdesc.db";
    DiskManager* dm = nullptr;
    BufferPoolManager* bpm = nullptr;
    BPlusTree* tree = SetupTree(fileName, dm, bpm);

    const int N = 500;
    for (int i = 1; i <= N; i++) {
        RID rid(0, static_cast<slot_id_t>(i));
        tree->insert(i, rid);
    }

    // Delete in descending order — stresses left-sibling borrow/merge paths
    for (int i = N; i >= 1; i--) {
        bool ok = tree->Remove(i);
        assert(ok && "AscInsDel: Remove returned false");
    }

    tree->VerifyLeafChain({});

    TeardownTree(fileName, tree, dm, bpm);
    std::cout << "[Passed] TestDeleteStress_AscInsDescDel\n\n";
}

// ============================================================================
// 10. DELETE — mixed insert-delete interleaved
// ============================================================================
void TestDeleteInterleaved() {
    std::cout << "[Running] TestDeleteInterleaved (insert/delete interleaved)...\n";
    std::string fileName = "test_del_interleaved.db";
    DiskManager* dm = nullptr;
    BufferPoolManager* bpm = nullptr;
    BPlusTree* tree = SetupTree(fileName, dm, bpm);

    // Insert 1..200, delete evens, verify only odds remain
    for (int i = 1; i <= 200; i++) {
        RID rid(0, static_cast<slot_id_t>(i));
        tree->insert(i, rid);
    }
    for (int i = 2; i <= 200; i += 2) {
        bool ok = tree->Remove(i);
        assert(ok && "Interleaved: Remove even failed");
    }

    std::vector<int32_t> expected;
    for (int i = 1; i <= 199; i += 2) expected.push_back(i);
    tree->VerifyLeafChain(expected);

    page_id_t root = GetRootId(bpm, 1);
    tree->ValidateTree(root, INT32_MIN, INT32_MAX);

    // Now re-insert evens
    for (int i = 2; i <= 200; i += 2) {
        RID rid(0, static_cast<slot_id_t>(i));
        tree->insert(i, rid);
    }

    expected.clear();
    for (int i = 1; i <= 200; i++) expected.push_back(i);
    tree->VerifyLeafChain(expected);

    root = GetRootId(bpm, 1);
    tree->ValidateTree(root, INT32_MIN, INT32_MAX);

    TeardownTree(fileName, tree, dm, bpm);
    std::cout << "[Passed] TestDeleteInterleaved\n\n";
}
// ============================================================================
// 11. RANGE SCAN — basic smoke test (keys 0..99, scan [25,75])
// ============================================================================
void RangeScanTest() {
    std::cout << "[Running] RangeScanTest (keys 0-99, scan [25,75])...\n";
    std::string fileName = "rangeScan.db";
    DiskManager* dm = nullptr;
    BufferPoolManager* bpm = nullptr;
    BPlusTree* tree = SetupTree(fileName, dm, bpm);

    for (int i = 0; i < 100; i++) {
        RID rid(0, static_cast<slot_id_t>(i));
        tree->insert(i, rid);
    }

    // Build expected: keys 25..75 inclusive → 51 RIDs
    std::vector<RID> expected;
    for (int i = 25; i <= 75; i++) {
        expected.push_back(RID(0, static_cast<slot_id_t>(i)));
    }

    std::vector<RID> actual = tree->RangeScan(25, 75);

    assert(actual.size() == expected.size() && "RangeScanTest: result count mismatch");
    // Verify each RID matches in order (leaf chain preserves sorted order)
    for (size_t idx = 0; idx < actual.size(); idx++) {
        assert(actual[idx] == expected[idx] && "RangeScanTest: RID mismatch at position");
    }

    TeardownTree(fileName, tree, dm, bpm);
    std::cout << "[Passed] RangeScanTest (51 RIDs returned correctly)\n\n";
}

// ============================================================================
// 12. RANGE SCAN — Day 15 Checkpoint
//     Insert keys 1..1000, RangeScan(250, 750) must return exactly 501 RIDs
//     (keys 250,251,...,750) in strictly ascending key order.
// ============================================================================
void TestRangeScan_Day15Checkpoint() {
    std::cout << "[Running] TestRangeScan_Day15Checkpoint (keys 1-1000, scan [250,750])...\n";
    std::string fileName = "test_rangescan_day15.db";
    DiskManager* dm = nullptr;
    BufferPoolManager* bpm = nullptr;
    BPlusTree* tree = SetupTree(fileName, dm, bpm);

    // ── Insert 1..1000 in sequential order ────────────────────────────────────
    for (int i = 1; i <= 1000; i++) {
        RID rid(static_cast<page_id_t>(i / 10),   // spread across fake pages
                static_cast<slot_id_t>(i % 10));
        tree->insert(i, rid);
    }

    // ── Verify full leaf chain is sorted ─────────────────────────────────────
    std::vector<int32_t> all_keys;
    for (int i = 1; i <= 1000; i++) all_keys.push_back(i);
    tree->VerifyLeafChain(all_keys);
    std::cout << "  [+] VerifyLeafChain passed (1000 keys in order).\n";

    // ── RangeScan(250, 750) ───────────────────────────────────────────────────
    std::vector<RID> results = tree->RangeScan(250, 750);

    // Checkpoint: exactly 501 RIDs (keys 250,251,...,750 inclusive)
    const int EXPECTED_COUNT = 501;  // 750 - 250 + 1
    assert(results.size() == static_cast<size_t>(EXPECTED_COUNT) &&
           "Day15: RangeScan(250,750) did not return 501 RIDs!");
    std::cout << "  [+] Count check passed: " << results.size() << " RIDs returned.\n";

    // Verify every returned RID corresponds to a key in [250,750] in sorted order
    for (int idx = 0; idx < EXPECTED_COUNT; idx++) {
        int expected_key = 250 + idx;  // keys 250,251,...,750
        // The RID we encoded: page_id = key/10, slot_id = key%10
        RID expected_rid(static_cast<page_id_t>(expected_key / 10),
                         static_cast<slot_id_t>(expected_key % 10));
        assert(results[idx] == expected_rid &&
               "Day15: RID at position does not match expected key mapping");
    }
    std::cout << "  [+] RID content check passed: all 501 RIDs match keys 250..750 in order.\n";

    // ── Edge cases ────────────────────────────────────────────────────────────
    // Scan a range with a single key
    std::vector<RID> single = tree->RangeScan(500, 500);
    assert(single.size() == 1 && "Day15: single-key scan must return exactly 1 RID");
    assert(single[0] == RID(50, 0) && "Day15: single-key RID wrong");
    std::cout << "  [+] Single-key scan [500,500] passed.\n";

    // Scan a range outside all keys
    std::vector<RID> empty_scan = tree->RangeScan(1001, 2000);
    assert(empty_scan.empty() && "Day15: out-of-range scan must return empty");
    std::cout << "  [+] Out-of-range scan [1001,2000] returned empty correctly.\n";

    // Validate full tree structure
    Page* hp = bpm->FetchPage(1);
    page_id_t root;
    std::memcpy(&root, hp->GetData(), sizeof(page_id_t));
    bpm->UnpinPage(1, false);
    tree->ValidateTree(root, INT32_MIN, INT32_MAX);
    std::cout << "  [+] ValidateTree passed — all BST invariants hold.\n";

    TeardownTree(fileName, tree, dm, bpm);
    std::cout << "[Passed] TestRangeScan_Day15Checkpoint ✓\n\n";
}

// ============================================================================
// 13. DAY 16 — Pin-leak audit
//     Every BTree method must leave pin_count == 0 on all frames when done.
// ============================================================================
void TestDay16_PinLeakAudit() {
    std::cout << "[Running] TestDay16_PinLeakAudit (500 keys, pin audit after every op)...\n";
    std::string fileName = "test_day16_pins.db";
    DiskManager* dm = nullptr;
    BufferPoolManager* bpm = nullptr;
    BPlusTree* tree = SetupTree(fileName, dm, bpm);

    for (int i = 1; i <= 500; i++) {
        RID rid(0, static_cast<slot_id_t>(i));
        tree->insert(i, rid);
    }
    bpm->CheckAllUnpinned();
    std::cout << "  [+] After 500 inserts: all frames unpinned.\n";

    for (int i = 1; i <= 500; i++) {
        std::vector<RID> r;
        bool found = tree->Search(i, &r);
        assert(found && "PinAudit: Search missed a key");
    }
    bpm->CheckAllUnpinned();
    std::cout << "  [+] After 500 searches: all frames unpinned.\n";

    std::vector<RID> scan = tree->RangeScan(1, 500);
    assert(scan.size() == 500 && "PinAudit: RangeScan count wrong");
    bpm->CheckAllUnpinned();
    std::cout << "  [+] After RangeScan(1,500): all frames unpinned.\n";

    for (int i = 1; i <= 250; i++) {
        bool ok = tree->Remove(i);
        assert(ok && "PinAudit: Remove returned false");
    }
    bpm->CheckAllUnpinned();
    std::cout << "  [+] After 250 deletes: all frames unpinned.\n";

    Page* hp = bpm->FetchPage(1);
    page_id_t root;
    std::memcpy(&root, hp->GetData(), sizeof(page_id_t));
    bpm->UnpinPage(1, false);
    tree->ValidateTree(root, INT32_MIN, INT32_MAX);
    bpm->CheckAllUnpinned();
    std::cout << "  [+] After ValidateTree: all frames unpinned.\n";

    TeardownTree(fileName, tree, dm, bpm);
    std::cout << "[Passed] TestDay16_PinLeakAudit\n\n";
}

// ============================================================================
// 14. DAY 16 — Restart persistence
//     Build tree with 500 keys, flush, destroy, reopen, verify Search works.
// ============================================================================
void TestDay16_RestartPersistence() {
    std::cout << "[Running] TestDay16_RestartPersistence (500 keys survive restart)...\n";
    const std::string DB_FILE = "test_day16_restart.db";
    const int N = 500;
    page_id_t header_id = INVALID_PAGE_ID;

    // Phase A: build and flush
    {
        std::remove(DB_FILE.c_str());
        DiskManager* dm = new DiskManager(DB_FILE);
        BufferPoolManager* bpm = new BufferPoolManager(dm);

        Page* raw_hdr = bpm->NewPage(&header_id);
        assert(raw_hdr != nullptr);
        bpm->UnpinPage(header_id, false);

        BPlusTree* tree = new BPlusTree(bpm, header_id);
        for (int i = 1; i <= N; i++) {
            RID rid(static_cast<page_id_t>(i / 10), static_cast<slot_id_t>(i % 10));
            tree->insert(i, rid);
        }
        bpm->flushAllPages();
        std::cout << "  [+] Phase A: " << N << " keys inserted and flushed (header=" << header_id << ").\n";
        delete tree; delete bpm; delete dm;
    }

    // Phase B: reopen and verify
    {
        DiskManager* dm = new DiskManager(DB_FILE);
        BufferPoolManager* bpm = new BufferPoolManager(dm);
        BPlusTree* tree = new BPlusTree(bpm, header_id);

        for (int i = 1; i <= N; i++) {
            std::vector<RID> r;
            assert(tree->Search(i, &r) && "Restart: key missing after reopen");
            RID expected(static_cast<page_id_t>(i / 10), static_cast<slot_id_t>(i % 10));
            assert(r[0] == expected && "Restart: RID value wrong after reopen");
        }
        bpm->CheckAllUnpinned();
        std::cout << "  [+] Phase B: all " << N << " keys verified after restart.\n";

        btreeMetaPage meta = tree->ReadMeta();
        assert(meta.root_page_id != INVALID_PAGE_ID && "Restart: root lost");
        assert(meta.total_key_count == N && "Restart: key count wrong");
        std::cout << "  [+] Meta after restart: root=" << meta.root_page_id
                  << "  keys=" << meta.total_key_count
                  << "  height=" << meta.tree_height << "\n";

        delete tree; delete bpm; delete dm;
        std::remove(DB_FILE.c_str());
    }
    std::cout << "[Passed] TestDay16_RestartPersistence\n\n";
}

// ============================================================================
// 15. DAY 16 — Metadata page struct
//     Build 300-key tree, read btreeMetaPage, assert all fields are correct.
// ============================================================================
void TestDay16_MetadataPage() {
    std::cout << "[Running] TestDay16_MetadataPage (300 keys, verify btreeMetaPage)...\n";
    std::string fileName = "test_day16_meta.db";
    DiskManager* dm = nullptr;
    BufferPoolManager* bpm = nullptr;
    BPlusTree* tree = SetupTree(fileName, dm, bpm);

    const int N = 300;
    for (int i = 1; i <= N; i++) {
        RID rid(0, static_cast<slot_id_t>(i));
        tree->insert(i, rid);
    }

    btreeMetaPage meta = tree->ReadMeta();

    assert(meta.root_page_id != INVALID_PAGE_ID && "Meta: root_page_id is INVALID");
    std::cout << "  [+] root_page_id = " << meta.root_page_id << "\n";

    assert(meta.total_key_count == N && "Meta: total_key_count mismatch");
    std::cout << "  [+] total_key_count = " << meta.total_key_count << " (expected " << N << ")\n";

    assert(meta.tree_height >= 1 && "Meta: tree_height must be >= 1");
    std::cout << "  [+] tree_height = " << meta.tree_height << "\n";

    bpm->CheckAllUnpinned();
    std::cout << "  [+] No pin leaks after ReadMeta().\n";

    Page* hp = bpm->FetchPage(1);
    page_id_t root;
    std::memcpy(&root, hp->GetData(), sizeof(page_id_t));
    bpm->UnpinPage(1, false);
    tree->ValidateTree(root, INT32_MIN, INT32_MAX);
    std::cout << "  [+] ValidateTree passed after metadata reads.\n";

    TeardownTree(fileName, tree, dm, bpm);
    std::cout << "[Passed] TestDay16_MetadataPage\n\n";
}

// ============================================================================
// MAIN EXECUTION ENTRY POINT
// ============================================================================
int main() {
    std::cout << "===========================================\n";
    std::cout << "STARTING B+ TREE FULL TEST SUITE\n";
    std::cout << "===========================================\n\n";

    try {
        // ── Insertion benchmarks (pre-existing) ───────────────────────────────
        TestRandomInsert();
        TestAscendingInsert();
        TestDescendingInsert();
        TestValidateTree();

        // ── Delete benchmarks ─────────────────────────────────────────────────
        TestDeleteCase1_NoUnderflow();
        TestDeleteCase2_BorrowFromSibling();
        TestDeleteCase3_MergeAndRootCollapse();
        TestDeleteStress_RandomOrder();
        TestDeleteStress_AscInsDescDel();
        TestDeleteInterleaved();

        // ── RangeScan (Day 15) ────────────────────────────────────────────────
        RangeScanTest();
        TestRangeScan_Day15Checkpoint();

        // ── Buffer pool wiring (Day 16) ───────────────────────────────────────
        TestDay16_PinLeakAudit();
        TestDay16_RestartPersistence();
        TestDay16_MetadataPage();

        std::cout << "===========================================\n";
        std::cout << "ALL TESTS PASSED SUCCESSFULLY!\n";
        std::cout << "===========================================\n";
    } catch (const std::exception& e) {
        std::cerr << "Test suite crashed with unexpected exception: " << e.what() << "\n";
        return 1;
    }

    return 0;
}