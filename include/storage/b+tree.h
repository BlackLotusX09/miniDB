#pragma once
#include "page.h"
#include "BufferPool.h"
#include "b_tree_internal_page.h"
#include "b_tree_leaf_page.h"
#include "rid.h"
#include <cassert>
#include <cstring>
#include <vector>
using namespace std;

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
    bool Search(int32_t key, vector<RID>* result);
    bool isLeaf(Page* page);
    void CreateLeafRoot(int32_t key, RID rid);
    void insert(int32_t key, RID rid);

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