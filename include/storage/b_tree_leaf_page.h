#pragma once
#include <cstdint>
#include <cstring>
#include "rid.h"




/**
 * ===================================================================================
 * B+ TREE LEAF PAGE MEMORY LAYOUT
 * ===================================================================================
 * A 4KB raw disk block storing sequentially packed (Key, Record ID) data pairs,
 * alongside a pointer to the next leaf node for fast sequential index scans.
 *
 * [----------------------- BTreePageHeader (20B total) ---------------------------]
 * +---------------+---------------+---------------+---------------+------------------+
 * |  page_id      |  page_type    |  num_keys     |  parent_id    |  next_leaf_id    |
 * |  (4 Bytes)    |  (4 Bytes)    |  (4 Bytes)    |  (4 Bytes)    |  (4 Bytes)       |
 * +---------------+---------------+---------------+---------------+------------------+
 * 0B              4B              8B              12B             16B                20B
 * [------ Packed (Key, RID) Pairs starting at offset 20 ------]
 * +---------------+-------------------------------+---------------+------------------+
 * |  key_0        |  rid_0 (page_id + slot_num)   |  key_1        |  ...             |
 * |  (4 Bytes)    |  (6 Bytes: 4B + 2B)           |  (4 Bytes)    |  ...             |
 * +---------------+-------------------------------+---------------+------------------+
 * 20B             24B                             30B             34B
 *
 * -----------------------------------------------------------------------------------
 * OFFSET ARITHMETIC FORMULAS:
 * * Header Fields:
 * - page_id:        data_ + 0
 * - page_type:      data_ + 4  (Leaf = 0)
 * - num_keys:       data_ + 8
 * - parent_id:      data_ + 12
 * - next_leaf_id:   data_ + 16   <- inside the 20B header
 * * Data Array (Starts at 20B, Entry size = 10B):
 * - Pair i start:   data_ + 20 + (i * 10)
 * - Key i:          data_ + 20 + (i * 10)
 * - RID i:          data_ + 20 + (i * 10) + 4
 * ===================================================================================
 */
class Page; // Forward declaration




class BTreeLeafPage {
public:
    explicit BTreeLeafPage(Page* page);
    explicit BTreeLeafPage(char* raw_data);

    // --- Header Getters/Setters ---
    int32_t GetPageId() const;
    void SetPageId(int32_t page_id);

    int GetNumKeys() const;
    void SetNumKeys(int num_keys);

    int32_t GetParentPageId() const;
    void SetParentPageId(int32_t parent_page_id);

    int32_t GetNextPageId() const;
    void SetNextPageId(int32_t next_page_id);
    int32_t GetPageType() const;
    void SetPageType(int32_t type);

    // --- Key/RID Getters/Setters ---
    int32_t GetKey(int i) const;
    RID GetRID(int i) const;
    void SetKeyRID(int i, int32_t key, RID rid);
    //search
    bool LeafSearch(int32_t key, RID* result);
    bool isFull();
    //insert
    void insert(int32_t key, RID rid);

    // --- Sizing Constants ---
    static constexpr size_t HEADER_SIZE = 20;
    // next_leaf_id is at offset 16, INSIDE the 20B header.
    // Data pairs start right after the full 20B header at offset 20.
    static constexpr size_t LEAF_HEADER_SIZE = HEADER_SIZE; // 20B - data starts here

    static constexpr size_t KEY_SIZE = sizeof(int32_t);
    static constexpr size_t RID_SIZE = 6;
    static constexpr size_t LEAF_PAIR_SIZE = KEY_SIZE + RID_SIZE; // 10B
    static constexpr size_t MAX_KEYS = 50;
private:
    char* data_;
};