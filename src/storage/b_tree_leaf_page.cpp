#include "b_tree_leaf_page.h"
#include "page.h"
#include <cstring>

BTreeLeafPage::BTreeLeafPage(Page* page) : data_(page->GetData()) {
    // Set page type flag to LEAF (e.g., 0) upon binding if initializing
    int32_t type = 0; 
    std::memcpy(data_ + 4, &type, sizeof(int32_t));
}

BTreeLeafPage::BTreeLeafPage(char* raw_data) : data_(raw_data) {}

// --- New Header Functionality: Reading directly from raw bytes ---

int32_t BTreeLeafPage::GetPageId() const {
    int32_t page_id;
    std::memcpy(&page_id, data_ + 0, sizeof(int32_t)); // Offset 0 is the start of the page
    return page_id;
}

void BTreeLeafPage::SetPageId(int32_t page_id) {
    std::memcpy(data_ + 0, &page_id, sizeof(int32_t));
}

int BTreeLeafPage::GetNumKeys() const {
    int num_keys;
    std::memcpy(&num_keys, data_ + 8, sizeof(int)); // Offset 8
    return num_keys;
}

void BTreeLeafPage::SetNumKeys(int num_keys) {
    std::memcpy(data_ + 8, &num_keys, sizeof(int));
}

int32_t BTreeLeafPage::GetParentPageId() const {
    int32_t parent_id;
    std::memcpy(&parent_id, data_ + 12, sizeof(int32_t)); // Offset 12
    return parent_id;
}

void BTreeLeafPage::SetParentPageId(int32_t parent_page_id) {
    std::memcpy(data_ + 12, &parent_page_id, sizeof(int32_t));
}

int32_t BTreeLeafPage::GetNextPageId() const {
    int32_t next_page_id;
    std::memcpy(&next_page_id, data_ + HEADER_SIZE, sizeof(int32_t)); // Offset 20
    return next_page_id;
}

void BTreeLeafPage::SetNextPageId(int32_t next_page_id) {
    std::memcpy(data_ + HEADER_SIZE, &next_page_id, sizeof(int32_t));
}

// --- Key/RID Processing ---

int32_t BTreeLeafPage::GetKey(int i) const {
    size_t off = LEAF_HEADER_SIZE + (i * LEAF_PAIR_SIZE);
    int32_t key;
    std::memcpy(&key, data_ + off, KEY_SIZE);
    return key;
}

RID BTreeLeafPage::GetRID(int i) const {
    size_t off = LEAF_HEADER_SIZE + (i * LEAF_PAIR_SIZE) + KEY_SIZE;
    RID rid;
    std::memcpy(&rid, data_ + off, RID_SIZE);
    return rid;
}

void BTreeLeafPage::SetKeyRID(int i, int32_t key, RID rid) {
    size_t pair_off = LEAF_HEADER_SIZE + (i * LEAF_PAIR_SIZE);
    std::memcpy(data_ + pair_off, &key, KEY_SIZE);
    std::memcpy(data_ + pair_off + KEY_SIZE, &rid, RID_SIZE);
}

RID BTreeLeafPage::LeafSearch(page_id_t pid, int32_t key){
    int num_keys = GetNumKeys();
    int low = 0;
    int high = num_keys-1;

    while(low<=high){
        int mid = (low+high)/2;
        int mid_key = GetKey(mid);
        if (mid_key == key) {
            return GetRID(mid);
        } 
        else if (mid_key > key) {
            high = mid - 1; // Move the ceiling down
        } 
        else {
            low = mid + 1;  // Move the floor up
        }
        
    }
    return RID{-1,-1};

}