#pragma once

#include <cstdint>
#include <cstring>
#include"page.h"


/**
 * ===================================================================================
 * B+ TREE INTERNAL PAGE MEMORY LAYOUT
 * ===================================================================================
 * A 4KB raw disk block interpreted as alternating Child Page IDs and Search Keys.
 * With N keys, there are always N + 1 child pointers.
 *
 * [---- BTreePageHeader (20B) ----][-- Pointers & Keys Alternating Layout... --------]
 * +---------------+---------------+---------------+---------------+------------------+
 * |  page_id      |  page_type    |  num_keys     |  parent_id    |  child_ptr_0     |
 * |  (4 Bytes)    |  (4 Bytes)    |  (4 Bytes)    |  (4 Bytes)    |  (4 Bytes)       |
 * +---------------+---------------+---------------+---------------+------------------+
 * 0B              4B              8B              12B             16B                20B
 * * +---------------+---------------+---------------+---------------+------------------+
 * |  key_0        |  child_ptr_1  |  key_1        |  child_ptr_2  |  ...             |
 * |  (4 Bytes)    |  (4 Bytes)    |  (4 Bytes)    |  (4 Bytes)    |  ...             |
 * +---------------+---------------+---------------+---------------+------------------+
 * 20B             24B             28B             32B             36B
 *
 * -----------------------------------------------------------------------------------
 * OFFSET ARITHMETIC FORMULAS:
 * * Header Fields:
 * - page_id:        data_ + 0
 * - page_type:      data_ + 4  (Internal = 1)
 * - num_keys:       data_ + 8
 * - parent_id:      data_ + 12
 * * Child Pointer i:    data_ + 16 + (i * 8)
 * Key i:              data_ + 20 + (i * 8)
 * ===================================================================================
 */
// Assuming a standard Page class exists with GetData()
class Page; 

class BTreeInternalPage {
public:
    explicit BTreeInternalPage(Page* page);

    // --- Header Getters/Setters ---
    int32_t GetPageId() const;
    void SetPageId(int32_t page_id);
    
    int GetNumKeys() const;
    void SetNumKeys(int num_keys);

    int32_t GetParentPageId() const;
    void SetParentPageId(int32_t parent_page_id);

    // --- Core Internal Node API ---
    int32_t GetKey(int i) const;
    void SetKey(int i, int32_t key);

    int32_t GetChildId(int i) const;
    void SetChildId(int i, int32_t child_id);

    // Look up the child page pointer that corresponds to a given key search
    int32_t Lookup(int32_t key) const;

    // Insert a new key and child pair right after an existing child pointer
    void InsertAfterChild(int32_t old_child, int32_t new_key, int32_t new_child);
    page_id_t InternalFindChild(page_id_t pid, int32_t key);

    bool IsFull() const;

    // Constants for internal layout arithmetic
    static constexpr size_t INTERNAL_HEADER_SIZE = 20; // 20B base header
    static constexpr size_t KEY_SIZE = sizeof(int32_t); // 4B
    static constexpr size_t CHILD_PTR_SIZE = sizeof(int32_t); // 4B
    static constexpr size_t MAX_KEYS = (4096 - INTERNAL_HEADER_SIZE - CHILD_PTR_SIZE) / (KEY_SIZE + CHILD_PTR_SIZE); 

private:
    char* data_;
};