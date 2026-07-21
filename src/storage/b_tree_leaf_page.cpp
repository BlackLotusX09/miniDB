#include "b_tree_leaf_page.h"
#include "page.h"
#include <cstring>

BTreeLeafPage::BTreeLeafPage(Page* page) : data_(page->GetData()) {
    // Only bind the data pointer — do NOT overwrite the page type here.
    // The caller is responsible for initializing the type via SetPageType().
    // Blindly writing type=0 here would corrupt internal pages fetched during traversal.
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
    // next_leaf_id lives at offset 16, inside the 20B base header (matches CreateLeafRoot)
    std::memcpy(&next_page_id, data_ + 16, sizeof(int32_t));
    return next_page_id;
}

void BTreeLeafPage::SetNextPageId(int32_t next_page_id) {
    std::memcpy(data_ + 16, &next_page_id, sizeof(int32_t));
}

// Returns the page type integer (0 = LEAF)
int32_t BTreeLeafPage::GetPageType() const {
    int32_t type;
    // Read 4 bytes starting at offset 4
    std::memcpy(&type, data_ + 4, sizeof(int32_t));
    return type;
}

// Sets the page type integer in the page header
void BTreeLeafPage::SetPageType(int32_t type) {
    // Write 4 bytes starting at offset 4
    std::memcpy(data_+ 4, &type, sizeof(int32_t));
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

bool BTreeLeafPage::LeafSearch(int32_t key, RID* result) {
    int num_keys = GetNumKeys();
    int low = 0;
    int high = num_keys - 1;

    while (low <= high) {
        int mid = low + (high - low) / 2; // Prevents potential overflow
        int mid_key = GetKey(mid);
        
        if (mid_key == key) {
            if (result != nullptr) {
                *result = GetRID(mid);
            }
            return true;
        } 
        else if (mid_key > key) {
            high = mid - 1; 
        } 
        else {
            low = mid + 1;  
        }
    }
    
    return false;
}
bool BTreeLeafPage::isFull() {
    int num_keys = GetNumKeys();
    return num_keys >= MAX_KEYS;
}
void BTreeLeafPage::insert(int32_t key, RID rid){
    if(!isFull()){
        int num_key = GetNumKeys();
        if (num_key == 0 || GetKey(num_key - 1) < key) {
            SetKeyRID(num_key, key, rid);
            SetNumKeys(num_key + 1);
            return;
        }
        int low = 0;
        int high = num_key - 1;
        while(low <= high){
            int mid = (low + high) / 2;
            int32_t mid_key = GetKey(mid);
            
            if(key == mid_key){
                SetKeyRID(mid, key, rid);
                return;
            }
            else if(key < mid_key){
                high = mid - 1;
            }
            else{
                low = mid + 1;
            }
        }
        
        int target_idx = low;

        for(int i = num_key - 1; i >= target_idx; i--){
            int32_t next_key = GetKey(i);
            RID next_rid = GetRID(i);
            SetKeyRID(i + 1, next_key, next_rid);
        }
        
        SetKeyRID(target_idx, key, rid);
        SetNumKeys(num_key + 1);
    }
}