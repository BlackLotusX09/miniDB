#include "storage/TableIterator.h"

TableIterator::TableIterator(BufferPoolManager* bpm, page_id_t start_page){
    this->bpm=bpm;
    this->page_id=start_page;
    current_page_ptr=bpm->FetchPage(start_page);
    this->slot_id=0;
}

bool TableIterator::Move(Tuple* tuple) {
    // If we are already at the end of the table, immediately stop
    if (page_id == INVALID_PAGE_ID || current_page_ptr == nullptr) {
        return false;
    }

    while (true) {
        SlottedPage* slp = reinterpret_cast<SlottedPage*>(current_page_ptr->GetData());
        
        // 1. If we are within bounds of the current page's slots
        if (slot_id < slp->Header()->num_slots) {
            uint16_t len = 0;
            const char* record = slp->GetRecord(slot_id, &len);
            
            if (record != nullptr) {
                // SUCCESS: Found a valid record!
                Tuple current_tuple(record, len);
                *tuple = current_tuple;
                
                slot_id++; // Move forward so the NEXT call checks the next slot
                return true;
            }
            
            // If record is nullptr, it's tombstoned/deleted. Just skip to the next slot!
            slot_id++;
            continue;
        }
        
        // 2. If we reach here, slot_id >= num_slots (End of the current page)
        page_id_t next_pid = slp->Header()->next_page_id;
        
        // Always unpin the current page before leaving it
        bpm->UnpinPage(page_id, false);
        
        if (next_pid == INVALID_PAGE_ID) {
            // We hit the absolute end of the database table
            page_id = INVALID_PAGE_ID;
            current_page_ptr = nullptr;
            slot_id = 0;
            return false;
        }
        
        // Move coordinates to the start of the next page and let the while loop check it
        page_id = next_pid;
        slot_id = 0;
        current_page_ptr = bpm->FetchPage(page_id);
    }
}