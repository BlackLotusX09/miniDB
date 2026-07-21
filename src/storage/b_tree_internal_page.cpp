#include "b_tree_internal_page.h"
#include <cassert>

BTreeInternalPage::BTreeInternalPage(Page* page) : data_(page->GetData()) {
    // Only bind the data pointer — do NOT overwrite the page type here.
    // The caller is responsible for initializing the type via SetPageType().
    // Blindly writing type=1 would corrupt leaf pages if ever wrapped by this class.
}

// --- Header Management ---
int32_t BTreeInternalPage::GetPageId() const {
    int32_t page_id;
    std::memcpy(&page_id, data_ + 0, sizeof(int32_t));
    return page_id;
}

void BTreeInternalPage::SetPageId(int32_t page_id) {
    std::memcpy(data_ + 0, &page_id, sizeof(int32_t));
}

int BTreeInternalPage::GetNumKeys() const {
    int num_keys;
    std::memcpy(&num_keys, data_ + 8, sizeof(int));
    return num_keys;
}

void BTreeInternalPage::SetNumKeys(int num_keys) {
    std::memcpy(data_ + 8, &num_keys, sizeof(int));
}

int32_t BTreeInternalPage::GetParentPageId() const {
    int32_t parent_id;
    std::memcpy(&parent_id, data_ + 12, sizeof(int32_t));
    return parent_id;
}

void BTreeInternalPage::SetParentPageId(int32_t parent_page_id) {
    std::memcpy(data_ + 12, &parent_page_id, sizeof(int32_t));
}

// --- Key & Child Pointer Getters/Setters ---

int32_t BTreeInternalPage::GetKey(int i) const {
    // Key i comes after child_0 and i full (child, key) pairs
    size_t off = INTERNAL_HEADER_SIZE + CHILD_PTR_SIZE + (i * (KEY_SIZE + CHILD_PTR_SIZE));
    int32_t key;
    std::memcpy(&key, data_ + off, KEY_SIZE);
    return key;
}

void BTreeInternalPage::SetKey(int i, int32_t key) {
    size_t off = INTERNAL_HEADER_SIZE + CHILD_PTR_SIZE + (i * (KEY_SIZE + CHILD_PTR_SIZE));
    std::memcpy(data_ + off, &key, KEY_SIZE);
}

int32_t BTreeInternalPage::GetChildId(int i) const {
    // Child i is right before Key i. For i=0, it's immediately after the header.
    size_t off = INTERNAL_HEADER_SIZE + (i * (KEY_SIZE + CHILD_PTR_SIZE));
    int32_t child_id;
    std::memcpy(&child_id, data_ + off, CHILD_PTR_SIZE);
    return child_id;
}

void BTreeInternalPage::SetChildId(int i, int32_t child_id) {
    size_t off = INTERNAL_HEADER_SIZE + (i * (KEY_SIZE + CHILD_PTR_SIZE));
    std::memcpy(data_ + off, &child_id, CHILD_PTR_SIZE);
}
// Returns the page type integer (1 = INTERNAL)
int32_t BTreeInternalPage::GetPageType() const {
    int32_t type;
    // Read 4 bytes starting at offset 4
    std::memcpy(&type, data_ + 4, sizeof(int32_t));
    return type;
}

// Sets the page type integer in the page header
void BTreeInternalPage::SetPageType(int32_t type) {
    // Write 4 bytes starting at offset 4
    std::memcpy(data_+ 4, &type, sizeof(int32_t));
}
// --- High Level Operations ---

bool BTreeInternalPage::IsFull() const {
    return GetNumKeys() >= static_cast<int>(MAX_KEYS);
}
void BTreeInternalPage::InsertAfterChild(page_id_t old_child, int32_t new_key, page_id_t new_child) {
    int size = GetNumKeys();
    int target_idx = -1;

    // Find the position of old_child among child pointers
    for (int i = 0; i <= size; ++i) {
        if (GetChildId(i) == old_child) {
            target_idx = i;
            break;
        }
    }
    assert(target_idx != -1 && "InsertAfterChild: old_child not found");

    // Shift keys right: key[target_idx..size-1] → key[target_idx+1..size]
    // Shift right children right: child[target_idx+1..size] → child[target_idx+2..size+1]
    for (int i = size - 1; i >= target_idx; --i) {
        SetKey(i + 1, GetKey(i));
        SetChildId(i + 2, GetChildId(i + 1));
    }

    // Place the new separator key and new right child
    SetKey(target_idx, new_key);
    SetChildId(target_idx + 1, new_child);

    SetNumKeys(size + 1);
}
page_id_t BTreeInternalPage::InternalFindChild(int32_t key){
    int num_keys = GetNumKeys();
    int low = 0;
    int high = num_keys-1;
    int ans=num_keys;
    page_id_t child_pid;
    while(low<=high){
        int mid = (low+high)/2;
        int key_mid = GetKey(mid);
        if(key_mid<=key){
            low=mid+1;
            
        }
        else{
            ans=mid;
            high=mid-1;
        }
    }
    child_pid = GetChildId(ans);
    return child_pid;
}
