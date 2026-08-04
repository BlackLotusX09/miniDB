#include "storage/TableIterator.h"
#include "storage/page.h"
#include <cstdint>

TableIterator::TableIterator(BufferPoolManager* bpm, page_id_t start_page){
    this->bpm_=bpm;
    this->page_id_=start_page;
    current_page_=bpm->FetchPage(start_page);
    this->slot_id_=0;
    AdvanceToValid();
}
TableIterator::TableIterator(){
    bpm_=nullptr;
    page_id_=INVALID_PAGE_ID;
    slot_id_ = 0;
    current_page_ = nullptr;
}
TableIterator::~TableIterator(){
    if (bpm_ != nullptr && page_id_ != INVALID_PAGE_ID && current_page_ != nullptr) {
        bpm_->UnpinPage(page_id_, false);
    }
}
void TableIterator::AdvanceToValid() {
    while (true) {                          // ← needs this loop
        if (page_id_ == INVALID_PAGE_ID) return;

        SlottedPage* slp = reinterpret_cast<SlottedPage*>(current_page_->GetData());

        if (slot_id_ < slp->Header()->num_slots) {
            uint16_t len;
            const char* record = slp->GetRecord(slot_id_, &len);
            if (record != nullptr) return;  // found a valid slot, stop
            slot_id_++;
            continue;                       // ← loop again to check next slot
        }

        // exhausted this page → move to next
        page_id_t next = slp->Header()->next_page_id;
        bpm_->UnpinPage(page_id_, false);
        if (next == INVALID_PAGE_ID) {
            page_id_ = INVALID_PAGE_ID;
            current_page_ = nullptr;
            return;
        }
        page_id_ = next;
        slot_id_ = 0;
        current_page_ = bpm_->FetchPage(page_id_);
        // loop again to check the new page
    }
}

const char* TableIterator::GetData() const{
    SlottedPage* slp = reinterpret_cast<SlottedPage*>(current_page_->GetData());
    uint16_t len;
    return slp->GetRecord(slot_id_, &len);
}
uint16_t TableIterator::GetLength() const {
    SlottedPage* slp = reinterpret_cast<SlottedPage*>(current_page_->GetData());
    uint16_t len;
    slp->GetRecord(slot_id_, &len);
    return len;
}

TableIterator& TableIterator::operator++(){
    slot_id_++;
    this->AdvanceToValid();
    return *this;
}

bool TableIterator::operator==(const TableIterator& other) const{
    return page_id_ == other.page_id_ && slot_id_ == other.slot_id_;
}
