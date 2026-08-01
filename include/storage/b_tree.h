#pragma once
#include "storage/page.h"
#include "storage/BufferPool.h"
#include "storage/b_tree_internal_page.h"
#include "storage/b_tree_leaf_page.h"
#include "storage/rid.h"
#include <cassert>
#include <cstring>
#include <vector>
using namespace std;

// ── Metadata page stored at header_page_id ────────────────────────────────────
// Layout (first 16 bytes of the header page):
//   [0..3]   root_page_id   (page_id_t)
//   [4..7]   total_key_count (int32_t)
//   [8..11]  tree_height     (int32_t)  — 0 = empty, 1 = leaf-only root
//   [12..15] reserved
struct btreeMetaPage {
    page_id_t root_page_id;    // ID of the current root node
    int32_t   total_key_count; // running total of inserted keys
    int32_t   tree_height;     // height of the tree (1 = single leaf)
    int32_t   reserved;
};

struct btreeHeader{
    page_id_t root_page_id;
    int32_t order;
};

class BPlusTree{
private:
    void InsertIntoParent(std::vector<page_id_t>& parent_stack, page_id_t old_child_id, int32_t push_up_key, page_id_t new_child_id);
    pair<int32_t, page_id_t> SplitLeaf(BTreeLeafPage& leafPage);
public:

    BPlusTree(BufferPoolManager* bpm, page_id_t headerPage);
    void UpdateParentIdOfPage(page_id_t page_id, page_id_t new_parent_id);
    page_id_t getRootPageId();
    void UpdateRootPageId(page_id_t new_root_id);
    bool Search(int32_t key, vector<RID>* result);
    bool isLeaf(Page* page);
    void CreateLeafRoot(int32_t key, RID rid);
    void insert(int32_t key, RID rid);
    //delete
    bool Remove(int32_t key);
    void BorrowFromRight(BTreeLeafPage* leaf, BTreeLeafPage* right, BTreeInternalPage* parent, int separator_idx);
    void BorrowFromLeft(BTreeLeafPage* leaf, BTreeLeafPage* left, BTreeInternalPage* parent, int separator_idx);
    void CoalesceOrRedistribute(page_id_t page_id);

    //Range Scan
    page_id_t findLeafContaining(int32_t low);
    vector<RID>RangeScan(int32_t low, int32_t high);

    // ── Day 16: metadata accessors ──────────────────────────────────────────────
    // Walks from root to leftmost leaf, counting levels.
    int GetTreeHeight();
    // Counts all keys by walking the full leaf chain.
    int GetKeyCount();
    // Reads the full btreeMetaPage from the header page.
    btreeMetaPage ReadMeta();
    // --- Validation helpers (Day 9-10 stress tests) ---
    // Recursively validates BST ordering invariants for every node in the subtree.
    // min_key/max_key are exclusive bounds (use INT32_MIN/INT32_MAX at root).
    void ValidateTree(page_id_t pid, int32_t min_key, int32_t max_key);

    // Follows next_leaf pointers from the leftmost leaf to the rightmost,
    // collects all keys, and verifies they are sorted and match expected_sorted_keys.
    void VerifyLeafChain(const std::vector<int32_t>& expected_sorted_keys);

private:
    BufferPoolManager* bpm_;
    page_id_t header_page_id;
};