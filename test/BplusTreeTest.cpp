
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
// MAIN EXECUTION ENTRY POINT
// ============================================================================
int main() {
    std::cout << "===========================================\n";
    std::cout << "STARTING B+ TREE INSERTION STRESS SUITE\n";
    std::cout << "===========================================\n\n";

    try {
        TestRandomInsert();
        TestAscendingInsert();
        TestDescendingInsert();
        TestValidateTree();

        std::cout << "===========================================\n";
        std::cout << "ALL TESTS PASSED SUCCESSFULLY!\n";
        std::cout << "===========================================\n";
    } catch (const std::exception& e) {
        std::cerr << "Test suite crashed with unexpected exception: " << e.what() << "\n";
        return 1;
    }

    return 0;
}