#include<TableHeap.h>

TableHeap::TableHeap(BufferPoolManager* buffer_pool_manager, page_id_t first_page_id){
    this->buffer_pool_manager=buffer_pool_manager;
    this->first_page_id=first_page_id;
}
bool TableHeap::InsertTuple(const Tuple& tuple, RID* rid, const Schema& schema){
    Page* page = buffer_pool_manager->FetchPage(last_page_id);
    SlottedPage* slp = reinterpret_cast<SlottedPage*>(page->GetData());
    slot_id_t out_slot;
    if(slp->InsertRecord(tuple.GetData(), tuple.GetLength(), &out_slot)){
        RID n_rid(last_page_id, out_slot);
        *rid = n_rid;
        buffer_pool_manager->UnpinPage(last_page_id, true); 
        return true;
    }
    
    page_id_t new_page_id;
    Page* new_page = buffer_pool_manager->NewPage(&new_page_id);
    
    SlottedPage* spage = reinterpret_cast<SlottedPage*>(new_page->GetData());
    auto* new_header = spage->Header();
    new_header->next_page_id = -1;
    new_header->page_id = new_page_id;
    
    auto* old_header = slp->Header();
    old_header->next_page_id = new_page_id;

    buffer_pool_manager->UnpinPage(last_page_id, true);
    this->last_page_id = new_page_id;
    if(spage->InsertRecord(tuple.GetData(), tuple.GetLength(), &out_slot)){
        RID n_rid(new_page_id, out_slot);
        *rid = n_rid;
        
        buffer_pool_manager->UnpinPage(new_page_id, true);
        return true;
    }
    
    buffer_pool_manager->UnpinPage(new_page_id, false);
    return false;
}
bool TableHeap::GetTuple(const RID& rid, Tuple* tuple, const Schema& schema){
    page_id_t page_id = rid.page_id;
    slot_id_t slot_id = rid.slot_id;
    
    Page* page = buffer_pool_manager->FetchPage(page_id);
    SlottedPage* slottedPage = reinterpret_cast<SlottedPage*>(page->GetData());
    
    uint16_t len;
    const char* record;
    
    if(record = slottedPage->GetRecord(slot_id, &len)){
        Tuple fetched_tuple(record, len);
        *tuple = fetched_tuple;
        
        buffer_pool_manager->UnpinPage(page_id, false);
        return true;
    }

    buffer_pool_manager->UnpinPage(page_id, false);
    return false;
}