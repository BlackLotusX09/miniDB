#include "storage/b+tree.h"



BPlusTree:: BPlusTree(BufferPoolManager* bpm, page_id_t headerPage)
    : header_page_id(headerPage), bpm_(bpm) {
    
    // 1. Fetch the master header page frame from the buffer pool
    Page* header_page = bpm_->FetchPage(header_page_id);
    if (header_page == nullptr) {
        // Handle critical buffer frame failure if necessary
        return; 
    }

    // 2. Read the first 4 bytes to check if it already has a root mapped
    page_id_t existing_root_id;
    std::memcpy(&existing_root_id, header_page->GetData(), sizeof(page_id_t));

    // 3. If the page contains uninitialized garbage or is completely fresh, cold-start it
    // Note: If your DiskManager zeroes out new pages, a fresh page will read as 0.
    // If 0 is a valid page ID in your system, check if it needs a explicit reset flag,
    // or initialize it to INVALID_PAGE_ID down on disk during the very first file creation.
    if (existing_root_id == 0 || existing_root_id == INVALID_PAGE_ID) {
        page_id_t initial_root_id = INVALID_PAGE_ID;
        std::memcpy(header_page->GetData(), &initial_root_id, sizeof(page_id_t));
        
        // Unpin as DIRTY so the initial INVALID_PAGE_ID (-1) state is flushed to disk
        bpm_->UnpinPage(header_page_id, true);
    } else {
        // The tree already exists from a previous run! Keep it pinned or unpin cleanly.
        bpm_->UnpinPage(header_page_id, false);
    }
}


bool BPlusTree::isLeaf(Page* page){
    int page_type;
    memcpy(&page_type,page->GetData()+sizeof(int32_t),sizeof(int32_t));
    if(page_type==0)return true;
    return false;
}
page_id_t BPlusTree::getRootPageId() {
    auto* header = bpm_->FetchPage(header_page_id);
    if (header == nullptr) return INVALID_PAGE_ID;

    // Get root page id of the tree from the header
    page_id_t root_page_id;
    memcpy(&root_page_id, header->GetData(), sizeof(page_id_t));
    
    bpm_->UnpinPage(header_page_id, false);

    return root_page_id;
}

void BPlusTree::UpdateRootPageId(page_id_t new_root_page_id) {
    auto* header = bpm_->FetchPage(header_page_id);
    if (header == nullptr) return;

    // Write new root page id into header page data
    memcpy(header->GetData(), &new_root_page_id, sizeof(page_id_t));
    
    // Mark dirty = true so the header update persists
    bpm_->UnpinPage(header_page_id, true);
}
void BPlusTree::UpdateParentIdOfPage(page_id_t page_id, page_id_t new_parent_id) {
    if (page_id == INVALID_PAGE_ID) return;
    
    Page* raw_page = bpm_->FetchPage(page_id);
    if (raw_page == nullptr) return;

    // Both structural classes map parent_id at Byte Offset 12 (12B) in their headers
    std::memcpy(raw_page->GetData() + 12, &new_parent_id, sizeof(page_id_t));
    
    bpm_->UnpinPage(page_id, true);
}

bool BPlusTree::Search(int32_t key, std::vector<RID>* result) {
    // 1. Fetch the master metadata header block
    auto* header_page = bpm_->FetchPage(header_page_id);
    if (header_page == nullptr) {
        return false;
    }

    // 2. Read the current root page ID out of the first 4 bytes
    page_id_t root_pid;
    std::memcpy(&root_pid, header_page->GetData(), sizeof(page_id_t));
    bpm_->UnpinPage(header_page_id, false);

    // 4. If the tree is empty and has no root yet, terminate search safely
    if (root_pid == INVALID_PAGE_ID) { 
        return false;
    }

    page_id_t page_id = root_pid;
    
    while (page_id != INVALID_PAGE_ID) { 
        auto* page = bpm_->FetchPage(page_id);
        if (page == nullptr) {
            return false;
        }

        if (isLeaf(page)) {
            BTreeLeafPage leaf(page);
            
            // Clean local search optimization
            RID rid;
            bool found = leaf.LeafSearch(key, &rid); 
            
            if (found && result != nullptr) {
                result->push_back(rid);
            }
            
            bpm_->UnpinPage(page_id, false);
            return found; 
        } else {
            BTreeInternalPage internalPage(page);
            // Internal find child relies entirely on standard key comparison logic
            page_id_t next = internalPage.InternalFindChild(key);
            
            bpm_->UnpinPage(page_id, false);
            page_id = next; 
        } 
    }
    
    return false;
}
void BPlusTree::CreateLeafRoot(int32_t key, RID rid) {
    page_id_t new_root_id;
    Page* raw_root = bpm_->NewPage(&new_root_id); // Allocate new frame from disk
    if (raw_root == nullptr) return; // Production guard

    // Setup initial leaf layout header flags
    int32_t page_type = 0; // 0 = LEAF
    int32_t initial_keys = 0;
    page_id_t parent_id = INVALID_PAGE_ID;
    page_id_t next_leaf = INVALID_PAGE_ID;
    
    std::memcpy(raw_root->GetData() + 0, &new_root_id, sizeof(page_id_t));
    std::memcpy(raw_root->GetData() + 4, &page_type, sizeof(int32_t));
    std::memcpy(raw_root->GetData() + 8, &initial_keys, sizeof(int32_t));
    std::memcpy(raw_root->GetData() + 12, &parent_id, sizeof(page_id_t));
    std::memcpy(raw_root->GetData() + 16, &next_leaf, sizeof(page_id_t));

    // Use your leaf page wrapper to insert the very first record pair
    BTreeLeafPage root_leaf(raw_root);
    root_leaf.insert(key, rid);
    bpm_->UnpinPage(new_root_id, true); // Mark dirty since keys changed

    // Update global master header tracking pointer
    auto* header = bpm_->FetchPage(header_page_id);
    if (header != nullptr) {
        std::memcpy(header->GetData(), &new_root_id, sizeof(page_id_t));
        bpm_->UnpinPage(header_page_id, true); // Mark header dirty!
    }
}
pair<int32_t, page_id_t> BPlusTree::SplitLeaf(BTreeLeafPage& leafPage) {
    page_id_t new_page_id;
    Page* new_page = bpm_->NewPage(&new_page_id);
    BTreeLeafPage new_leaf_page(new_page);
    
    // Initialize layout flags/metadata
    new_leaf_page.SetPageId(new_page_id);
    new_leaf_page.SetPageType(0); // 0 = LEAF

    int num_keys = leafPage.GetNumKeys();
    int upper_half = num_keys / 2;
    
    // Redistribute items to the upper sibling half
    int new_idx = 0;
    for (int idx = upper_half; idx < num_keys; idx++) {
        int32_t old_key = leafPage.GetKey(idx);
        RID old_rid = leafPage.GetRID(idx);
        new_leaf_page.SetKeyRID(new_idx++, old_key, old_rid);
    }
    
    // Recalculate sizes
    leafPage.SetNumKeys(upper_half);
    new_leaf_page.SetNumKeys(num_keys - upper_half);

    int32_t push_up_key = new_leaf_page.GetKey(0);

    // Patch the linked list chain
    new_leaf_page.SetNextPageId(leafPage.GetNextPageId());
    leafPage.SetNextPageId(new_page_id);

    // Safely drop the pin count for the newly written page
    bpm_->UnpinPage(new_page_id, true);

    // Return the data tokens back to the orchestration method
    return {push_up_key, new_page_id};
}


void BPlusTree::InsertIntoParent(std::vector<page_id_t>& parent_stack, page_id_t old_child_id, int32_t push_up_key, page_id_t new_child_id) {
    page_id_t curr_old_child = old_child_id;
    page_id_t curr_new_child = new_child_id;
    int32_t curr_key = push_up_key;

    while (true) {
        // CASE 1: Root Split Case (We hit the top of the tree)
        if (parent_stack.empty()) {
            page_id_t new_root_id;
            Page* raw_root = bpm_->NewPage(&new_root_id);
            BTreeInternalPage new_root(raw_root);
            
            new_root.SetPageId(new_root_id);
            new_root.SetPageType(1); // 1 = INTERNAL
            new_root.SetNumKeys(0);

            // Configure pointers for the two split subtrees below the new root
            new_root.SetChildId(0, curr_old_child);
            new_root.InsertAfterChild(curr_old_child, curr_key, curr_new_child);

            // Update parent_id fields in both child blocks to point to the new root
            UpdateParentIdOfPage(curr_old_child, new_root_id);
            UpdateParentIdOfPage(curr_new_child, new_root_id);

            // Persist the root_page_id durably in the master header page
            auto* header = bpm_->FetchPage(header_page_id);
            std::memcpy(header->GetData(), &new_root_id, sizeof(page_id_t));
            bpm_->UnpinPage(header_page_id, true);
            
            bpm_->UnpinPage(new_root_id, true);
            break; // Tree grew by 1 level successfully!
        }

        // Fetch the immediate parent node from the stack path
        page_id_t parent_id = parent_stack.back();
        parent_stack.pop_back(); // Pop it since we are processing it now
        
        Page* raw_internal_page = bpm_->FetchPage(parent_id);
        BTreeInternalPage internalPage(raw_internal_page);

        // Stage 1: Always insert the child pair into the internal node first
        internalPage.InsertAfterChild(curr_old_child, curr_key, curr_new_child);
        UpdateParentIdOfPage(curr_new_child, parent_id);

        // CASE 2: The parent internal page has enough space
        if (!internalPage.IsFull()) {
            bpm_->UnpinPage(parent_id, true);
            break; // Insertion chain completely finished safely!
        }

        // CASE 3: The parent internal page is full -> Trigger Internal Node Split!
        page_id_t new_parent_id;
        Page* raw_new_parent = bpm_->NewPage(&new_parent_id);
        BTreeInternalPage new_internal_page(raw_new_parent);
        
        new_internal_page.SetPageId(new_parent_id);
        new_internal_page.SetPageType(1); // 1 = INTERNAL

        int num_keys = internalPage.GetNumKeys();
        int mid_idx = num_keys / 2; // Midpoint key index to be pushed up

        // The middle key is MOVED up, so the new parent's key tracking elements 
        // start moving from mid_idx + 1 up to the total capacity count bounds
        int32_t next_level_key = internalPage.GetKey(mid_idx);

        // The new node's first child is the right child of the pushed-up key
        new_internal_page.SetChildId(0, internalPage.GetChildId(mid_idx + 1));
        UpdateParentIdOfPage(internalPage.GetChildId(mid_idx + 1), new_parent_id);

        // Copy remaining keys and their right children to the new sibling
        int new_idx = 0;
        for (int idx = mid_idx + 1; idx < num_keys; idx++) {
            new_internal_page.SetKey(new_idx, internalPage.GetKey(idx));
            new_internal_page.SetChildId(new_idx + 1, internalPage.GetChildId(idx + 1));
            UpdateParentIdOfPage(internalPage.GetChildId(idx + 1), new_parent_id);
            new_idx++;
        }

        // Adjust size headers for both internal nodes
        internalPage.SetNumKeys(mid_idx);
        new_internal_page.SetNumKeys(num_keys - mid_idx - 1);

        bpm_->UnpinPage(parent_id, true);
        bpm_->UnpinPage(new_parent_id, true);

        // Update tracking tokens to loop back up to the grandparent level
        curr_old_child = parent_id;
        curr_new_child = new_parent_id;
        curr_key = next_level_key;
    }
}

void BPlusTree::insert(int32_t key, RID rid){
    page_id_t root_page_id = getRootPageId();

    // 2. Edge Case: If tree is completely empty, create the first leaf root node
    if (root_page_id == INVALID_PAGE_ID) {
       CreateLeafRoot(key,rid);
       return;
    }

    // 3. Normal Downward Traversal
    std::vector<page_id_t> parent_stack;
    page_id_t page_id = root_page_id;

    while (page_id != INVALID_PAGE_ID) {
        Page* page = bpm_->FetchPage(page_id);
        if (page == nullptr) return;

        if (!isLeaf(page)) {
            BTreeInternalPage internalPage(page);
            parent_stack.push_back(page_id);
            
            page_id_t new_page_id = internalPage.InternalFindChild(key);
            bpm_->UnpinPage(page_id, false);
            
            page_id = new_page_id; // Fix: Advance loop tracking down a level
        } 
        else {
            BTreeLeafPage leafPage(page);
            leafPage.insert(key, rid);
            
            if (leafPage.isFull()) {
                // Delegate split actions entirely to cleanly separated helpers
                auto [push_up_key, new_page_id] = SplitLeaf(leafPage);
                InsertIntoParent(parent_stack, page_id, push_up_key, new_page_id);
            } 
            
            bpm_->UnpinPage(page_id, true); 
            break; 
        }
    }
}

//delete

// ============================================================================
// Leaf-level borrow helpers (unchanged API, used by Remove)
// ============================================================================

void BPlusTree::BorrowFromRight(BTreeLeafPage* leaf, BTreeLeafPage* right,
                                 BTreeInternalPage* parent, int separator_idx) {
    // Move first entry of right sibling to end of leaf
    leaf->insert(right->GetKey(0), right->GetRID(0));
    right->deleteAt(0);
    // Update separator in parent to new first key of right sibling
    parent->SetKey(separator_idx, right->GetKey(0));
}

void BPlusTree::BorrowFromLeft(BTreeLeafPage* leaf, BTreeLeafPage* left,
                                BTreeInternalPage* parent, int separator_idx) {
    int last_idx  = left->GetNumKeys() - 1;
    int32_t moved_key = left->GetKey(last_idx);
    RID     moved_rid = left->GetRID(last_idx);
    leaf->insert(moved_key, moved_rid);
    left->deleteAt(last_idx);
    // The separator in the parent must now equal the key we moved up
    parent->SetKey(separator_idx, moved_key);
}

// ============================================================================
// CoalesceOrRedistribute — kept for header compatibility, delegates to Remove
// ============================================================================
void BPlusTree::CoalesceOrRedistribute(page_id_t /*page_id*/) {
    // Intentionally left as a no-op stub.
    // All merge/borrow logic is handled inside Remove().
}

// ============================================================================
// Remove — complete implementation with full upward cascade
// Cases handled:
//   1. Leaf has enough keys after deletion        → done
//   2. Borrow from right leaf sibling             → update separator, done
//   3. Borrow from left  leaf sibling             → update separator, done
//   4. Merge leaf (right into left)               → remove separator from parent,
//                                                   cascade upward via internal loop
//   5. Internal node borrow from right sibling    → pull separator down, push up
//   6. Internal node borrow from left  sibling    → pull separator down, push up
//   7. Internal node merge                        → pull separator down into left,
//                                                   remove from grandparent, cascade
//   8. Root underflow (root has 0 keys, 1 child)  → make child the new root
// ============================================================================
bool BPlusTree::Remove(int32_t key) {

    page_id_t root_page_id = getRootPageId();
    if (root_page_id == INVALID_PAGE_ID) return false;

    // ── Phase 1: descend to the leaf, recording the path ──────────────────────
    std::vector<page_id_t> parent_stack; // ancestor page IDs, root first
    page_id_t current_id = root_page_id;
    Page* raw_page = bpm_->FetchPage(current_id);
    if (raw_page == nullptr) return false;

    while (!isLeaf(raw_page)) {
        BTreeInternalPage internalPage(raw_page);
        parent_stack.push_back(current_id);
        page_id_t next_id = internalPage.InternalFindChild(key);
        bpm_->UnpinPage(current_id, false);
        current_id = next_id;
        raw_page = bpm_->FetchPage(current_id);
        if (raw_page == nullptr) return false;
    }

    // ── Phase 2: delete from the leaf ─────────────────────────────────────────
    page_id_t leaf_page_id = current_id;
    BTreeLeafPage leaf(raw_page);

    int index = leaf.LookUp(key);
    if (index == -1) {
        bpm_->UnpinPage(leaf_page_id, false);
        return false; // key not present
    }
    leaf.deleteAt(index);

    // Case 1 — leaf has enough keys (or IS the root leaf)
    if (leaf.GetNumKeys() >= leaf.GetMinSize() || parent_stack.empty()) {
        bpm_->UnpinPage(leaf_page_id, true);
        return true;
    }

    // ── Phase 3: leaf underflow → borrow or merge ─────────────────────────────
    page_id_t parent_page_id = parent_stack.back();
    parent_stack.pop_back();

    Page* raw_parent = bpm_->FetchPage(parent_page_id);
    if (raw_parent == nullptr) { bpm_->UnpinPage(leaf_page_id, true); return true; }
    BTreeInternalPage parentPage(raw_parent);

    int child_idx = parentPage.ValueIndex(leaf_page_id);

    // --- Try borrow from right leaf sibling ---
    if (child_idx + 1 <= parentPage.GetNumKeys()) {
        page_id_t right_id = parentPage.GetChildId(child_idx + 1);
        Page* raw_right = bpm_->FetchPage(right_id);
        BTreeLeafPage rightPage(raw_right);

        if (rightPage.GetNumKeys() > rightPage.GetMinSize()) {
            BorrowFromRight(&leaf, &rightPage, &parentPage, child_idx);
            bpm_->UnpinPage(right_id,       true);
            bpm_->UnpinPage(parent_page_id, true);
            bpm_->UnpinPage(leaf_page_id,   true);
            return true;
        }
        bpm_->UnpinPage(right_id, false);
    }

    // --- Try borrow from left leaf sibling ---
    if (child_idx - 1 >= 0) {
        page_id_t left_id = parentPage.GetChildId(child_idx - 1);
        Page* raw_left = bpm_->FetchPage(left_id);
        BTreeLeafPage leftPage(raw_left);

        if (leftPage.GetNumKeys() > leftPage.GetMinSize()) {
            BorrowFromLeft(&leaf, &leftPage, &parentPage, child_idx - 1);
            bpm_->UnpinPage(left_id,        true);
            bpm_->UnpinPage(parent_page_id, true);
            bpm_->UnpinPage(leaf_page_id,   true);
            return true;
        }
        bpm_->UnpinPage(left_id, false);
    }

    // --- Merge (neither sibling could lend) ---
    // Convention: always merge RIGHT into LEFT to keep the linked list simple.
    // If there is a right sibling, merge it into 'leaf'.
    // Otherwise merge 'leaf' into the left sibling.

    if (child_idx + 1 <= parentPage.GetNumKeys()) {
        // --- Merge right sibling INTO current leaf ---
        page_id_t right_id = parentPage.GetChildId(child_idx + 1);
        Page* raw_right = bpm_->FetchPage(right_id);
        BTreeLeafPage rightPage(raw_right);

        int base = leaf.GetNumKeys();
        int r    = rightPage.GetNumKeys();
        for (int i = 0; i < r; ++i)
            leaf.SetKeyRID(base + i, rightPage.GetKey(i), rightPage.GetRID(i));
        leaf.SetNumKeys(base + r);
        leaf.SetNextPageId(rightPage.GetNextPageId());

        bpm_->UnpinPage(leaf_page_id, true);
        bpm_->UnpinPage(right_id,     false); // right page is now dead

        // Remove separator at child_idx (separator between leaf and right)
        parentPage.removeAt(child_idx);

    } else {
        // --- Merge current leaf INTO left sibling ---
        page_id_t left_id = parentPage.GetChildId(child_idx - 1);
        Page* raw_left = bpm_->FetchPage(left_id);
        BTreeLeafPage leftPage(raw_left);

        int base = leftPage.GetNumKeys();
        int r    = leaf.GetNumKeys();
        for (int i = 0; i < r; ++i)
            leftPage.SetKeyRID(base + i, leaf.GetKey(i), leaf.GetRID(i));
        leftPage.SetNumKeys(base + r);
        leftPage.SetNextPageId(leaf.GetNextPageId());

        bpm_->UnpinPage(left_id,      true);
        bpm_->UnpinPage(leaf_page_id, false); // current leaf is now dead

        // Remove separator at child_idx - 1
        parentPage.removeAt(child_idx - 1);
    }

    // ── Phase 4: cascade upward through internal nodes ────────────────────────
    // parentPage is still pinned (dirty). Check if it now underflows.

    page_id_t cur_node_id = parent_page_id;

    while (true) {
        // Reload the current underflowing internal node
        Page* raw_cur = bpm_->FetchPage(cur_node_id);
        BTreeInternalPage curNode(raw_cur);
        int cur_keys = curNode.GetNumKeys();

        // --- Case: this IS the root ---
        if (cur_node_id == getRootPageId()) {
            if (cur_keys == 0) {
                // Root underflow: promote the single remaining child
                page_id_t new_root = curNode.GetChildId(0);
                bpm_->UnpinPage(cur_node_id, false);
                UpdateRootPageId(new_root);
                // Clear the new root's parent pointer
                UpdateParentIdOfPage(new_root, INVALID_PAGE_ID);
            } else {
                bpm_->UnpinPage(cur_node_id, false);
            }
            bpm_->UnpinPage(parent_page_id, true); // flush the original parent write
            return true;
        }

        int min_internal = (static_cast<int>(BTreeInternalPage::MAX_KEYS) + 1) / 2;

        // No underflow at this level — we are done
        if (cur_keys >= min_internal) {
            bpm_->UnpinPage(cur_node_id,    false);
            bpm_->UnpinPage(parent_page_id, true);
            return true;
        }

        // Underflow! Fetch grandparent
        page_id_t grand_id;
        if (parent_stack.empty()) {
            // cur_node is the root (edge case — handled above, but be safe)
            bpm_->UnpinPage(cur_node_id,    false);
            bpm_->UnpinPage(parent_page_id, true);
            return true;
        }
        grand_id = parent_stack.back();
        parent_stack.pop_back();

        Page* raw_grand = bpm_->FetchPage(grand_id);
        BTreeInternalPage grandPage(raw_grand);

        int cur_child_idx = grandPage.ValueIndex(cur_node_id);

        // ---- Try borrow from right internal sibling ----
        if (cur_child_idx + 1 <= grandPage.GetNumKeys()) {
            page_id_t right_sib_id = grandPage.GetChildId(cur_child_idx + 1);
            Page* raw_rsib = bpm_->FetchPage(right_sib_id);
            BTreeInternalPage rightSib(raw_rsib);

            if (rightSib.GetNumKeys() > min_internal) {
                // Pull separator key DOWN from grandparent into curNode
                curNode.SetKey(cur_keys, grandPage.GetKey(cur_child_idx));
                // Move right sibling's first child pointer to curNode
                curNode.SetChildId(cur_keys + 1, rightSib.GetChildId(0));
                UpdateParentIdOfPage(rightSib.GetChildId(0), cur_node_id);
                curNode.SetNumKeys(cur_keys + 1);

                // Push right sibling's first key UP to grandparent separator
                grandPage.SetKey(cur_child_idx, rightSib.GetKey(0));

                // Shift right sibling left (remove its first key+child0)
                rightSib.removeAt(0);

                bpm_->UnpinPage(right_sib_id, true);
                bpm_->UnpinPage(grand_id,     true);
                bpm_->UnpinPage(cur_node_id,  true);
                bpm_->UnpinPage(parent_page_id, true);
                return true;
            }
            bpm_->UnpinPage(right_sib_id, false);
        }

        // ---- Try borrow from left internal sibling ----
        if (cur_child_idx - 1 >= 0) {
            page_id_t left_sib_id = grandPage.GetChildId(cur_child_idx - 1);
            Page* raw_lsib = bpm_->FetchPage(left_sib_id);
            BTreeInternalPage leftSib(raw_lsib);

            if (leftSib.GetNumKeys() > min_internal) {
                int L = leftSib.GetNumKeys();
                // Pull separator DOWN from grandparent to front of curNode
                curNode.InsertAtFront(grandPage.GetKey(cur_child_idx - 1),
                                      leftSib.GetChildId(L));
                UpdateParentIdOfPage(leftSib.GetChildId(L), cur_node_id);

                // Push left sibling's last key UP to grandparent separator
                grandPage.SetKey(cur_child_idx - 1, leftSib.GetKey(L - 1));

                // Shrink left sibling
                leftSib.SetNumKeys(L - 1);

                bpm_->UnpinPage(left_sib_id,    true);
                bpm_->UnpinPage(grand_id,        true);
                bpm_->UnpinPage(cur_node_id,     true);
                bpm_->UnpinPage(parent_page_id,  true);
                return true;
            }
            bpm_->UnpinPage(left_sib_id, false);
        }

        // ---- Internal merge: merge cur_node into left OR merge right into cur_node ----
        if (cur_child_idx + 1 <= grandPage.GetNumKeys()) {
            // Merge right sibling INTO cur_node
            page_id_t right_sib_id = grandPage.GetChildId(cur_child_idx + 1);
            Page* raw_rsib = bpm_->FetchPage(right_sib_id);
            BTreeInternalPage rightSib(raw_rsib);

            int R = rightSib.GetNumKeys();
            // Pull separator DOWN into cur_node
            curNode.SetKey(cur_keys, grandPage.GetKey(cur_child_idx));
            curNode.SetChildId(cur_keys + 1, rightSib.GetChildId(0));
            UpdateParentIdOfPage(rightSib.GetChildId(0), cur_node_id);
            for (int i = 0; i < R; ++i) {
                curNode.SetKey(cur_keys + 1 + i, rightSib.GetKey(i));
                curNode.SetChildId(cur_keys + 2 + i, rightSib.GetChildId(i + 1));
                UpdateParentIdOfPage(rightSib.GetChildId(i + 1), cur_node_id);
            }
            curNode.SetNumKeys(cur_keys + 1 + R);

            bpm_->UnpinPage(right_sib_id, false); // dead page
            bpm_->UnpinPage(cur_node_id,  true);

            // Remove separator at cur_child_idx from grandparent
            grandPage.removeAt(cur_child_idx);

        } else {
            // Merge cur_node INTO left sibling
            page_id_t left_sib_id = grandPage.GetChildId(cur_child_idx - 1);
            Page* raw_lsib = bpm_->FetchPage(left_sib_id);
            BTreeInternalPage leftSib(raw_lsib);

            int L = leftSib.GetNumKeys();
            // Pull separator DOWN into left sibling
            leftSib.SetKey(L, grandPage.GetKey(cur_child_idx - 1));
            leftSib.SetChildId(L + 1, curNode.GetChildId(0));
            UpdateParentIdOfPage(curNode.GetChildId(0), left_sib_id);
            for (int i = 0; i < cur_keys; ++i) {
                leftSib.SetKey(L + 1 + i, curNode.GetKey(i));
                leftSib.SetChildId(L + 2 + i, curNode.GetChildId(i + 1));
                UpdateParentIdOfPage(curNode.GetChildId(i + 1), left_sib_id);
            }
            leftSib.SetNumKeys(L + 1 + cur_keys);

            bpm_->UnpinPage(cur_node_id,  false); // dead page
            bpm_->UnpinPage(left_sib_id,  true);

            // Remove separator at cur_child_idx - 1 from grandparent
            grandPage.removeAt(cur_child_idx - 1);
        }

        // Grand page now has one fewer key — loop upward
        bpm_->UnpinPage(parent_page_id, true); // flush previous level's write
        parent_page_id = grand_id;             // grandparent becomes the new "parent"
        cur_node_id    = grand_id;
        // (grand_id page is still pinned — we'll unpin it at the top of the next iteration)
    }
}
// ============================================================================
// VALIDATION HELPERS (Day 9-10)
// ============================================================================

/**
 * Recursively traverses the B+ tree rooted at `pid` and asserts that:
 * - Every key in a leaf satisfies: min_key <= key < max_key
 * - Keys within a leaf are strictly increasing
 * - For each internal separator key[i], all keys in child[i] < key[i]
 *   and all keys in child[i+1] >= key[i]
 *
 * Call at root with (root_pid, INT32_MIN, INT32_MAX).
 */
void BPlusTree::ValidateTree(page_id_t pid, int32_t min_key, int32_t max_key) {
    if (pid == INVALID_PAGE_ID) return;

    auto* page = bpm_->FetchPage(pid);
    assert(page != nullptr && "ValidateTree: FetchPage returned null");

    if (isLeaf(page)) {
        BTreeLeafPage leaf(page);
        int n = leaf.GetNumKeys();
        for (int i = 0; i < n; i++) {
            int32_t k = leaf.GetKey(i);
            assert(k >= min_key && "ValidateTree: leaf key below lower bound");
            assert(k < max_key  && "ValidateTree: leaf key at or above upper bound");
            // Strict sort order within the leaf
            assert((i == 0 || leaf.GetKey(i) > leaf.GetKey(i-1)) &&
                   "ValidateTree: leaf keys not strictly ascending");
        }
        bpm_->UnpinPage(pid, false);
    } else {
        BTreeInternalPage node(page);
        int n = node.GetNumKeys();

        int32_t cur_min = min_key;
        for (int i = 0; i < n; i++) {
            int32_t sep = node.GetKey(i);
            // All keys in child[i] must be in [cur_min, sep)
            ValidateTree(node.GetChildId(i), cur_min, sep);
            cur_min = sep;
        }
        // All keys in the rightmost child must be in [cur_min, max_key)
        ValidateTree(node.GetChildId(n), cur_min, max_key);

        bpm_->UnpinPage(pid, false);
    }
}

/**
 * Finds the leftmost leaf by descending through the tree, then follows
 * next_leaf_id pointers across all leaf pages, collecting every key.
 * Asserts:
 * - Keys are strictly sorted across the entire chain
 * - The full collected set exactly matches expected_sorted_keys
 */
void BPlusTree::VerifyLeafChain(const std::vector<int32_t>& expected_sorted_keys) {
    auto* header_page = bpm_->FetchPage(header_page_id);
    assert(header_page != nullptr);
    page_id_t pid;
    std::memcpy(&pid, header_page->GetData(), sizeof(page_id_t));
    bpm_->UnpinPage(header_page_id, false);

    if (pid == INVALID_PAGE_ID) {
        assert(expected_sorted_keys.empty() &&
               "VerifyLeafChain: tree empty but expected keys non-empty");
        return;
    }

    // Descend to the leftmost leaf
    while (true) {
        auto* page = bpm_->FetchPage(pid);
        assert(page != nullptr);
        if (isLeaf(page)) {
            bpm_->UnpinPage(pid, false);
            break;
        }
        BTreeInternalPage node(page);
        page_id_t first_child = node.GetChildId(0);
        bpm_->UnpinPage(pid, false);
        pid = first_child;
    }

    // Walk the leaf chain and collect all keys
    std::vector<int32_t> collected;
    while (pid != INVALID_PAGE_ID) {
        auto* page = bpm_->FetchPage(pid);
        assert(page != nullptr);
        BTreeLeafPage leaf(page);
        int n = leaf.GetNumKeys();
        for (int i = 0; i < n; i++) {
            int32_t k = leaf.GetKey(i);
            assert((collected.empty() || k > collected.back()) &&
                   "VerifyLeafChain: keys not strictly ascending across leaf chain");
            collected.push_back(k);
        }
        page_id_t next = leaf.GetNextPageId();
        bpm_->UnpinPage(pid, false);
        pid = next;
    }

    assert(collected == expected_sorted_keys &&
           "VerifyLeafChain: collected keys do not match expected sorted keys");
}
