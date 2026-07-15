#include"include/storage/b+tree.h"
bool BPlusTree::isLeaf(Page* page){
    int page_type;
    memcpy(&page_type,page->GetData()+sizeof(int32_t),sizeof(int32_t));
    if(page_type==0)return true;
    return false;
}
RID BPlusTree::Search(int32_t key) {
    // 1. Fetch the master metadata header block
    auto* header_page = bpm_->FetchPage(header_page_id);
    if (header_page == nullptr) {
        return RID{-1, -1};
    }

    // 2. Read the current root page ID out of the first 4 bytes
    page_id_t root_pid;
    std::memcpy(&root_pid, header_page->GetData(), sizeof(page_id_t));

    // 3. Release the header page back to the buffer pool immediately
    bpm_->UnpinPage(header_page_id, false);

    // 4. If the tree is empty and has no root yet, terminate search safely
    if (root_pid == -1) { // Or your project's equivalent INVALID_PAGE_ID constant
        return RID{-1, -1};
    }

    // 5. Begin down-tree traversal starting from the root
    page_id_t page_id = root_pid;
    
    while (page_id != -1) { 
        auto* page = bpm_->FetchPage(page_id);
        if (page == nullptr) {
            return RID{-1, -1};
        }

        if (isLeaf(page)) {
            BTreeLeafPage leaf(page);
            RID result = leaf.LeafSearch(page_id, key);
            
            bpm_->UnpinPage(page_id, false);
            return result; // Terminal search complete
        }
        else {
            BTreeInternalPage internalPage(page);
            page_id_t next = internalPage.InternalFindChild(page_id, key);
            
            bpm_->UnpinPage(page_id, false);
            page_id = next; // Advance down to child level
        } 
    }
    
    return RID{-1, -1};
}