#include "storage/TableHeap.h"
#include "storage/page.h"
#include <cstring>

// ─── Constructor: create a brand-new table ────────────────────────────────────
TableHeap::TableHeap(BufferPoolManager* bpm) : buffer_pool_manager(bpm) {
    page_id_t new_pid;
    Page* page = buffer_pool_manager->NewPage(&new_pid);
    if (page == nullptr) {
        // Should never happen on a fresh buffer pool — treat as fatal
        first_page_id = INVALID_PAGE_ID;
        last_page_id  = INVALID_PAGE_ID;
        return;
    }

    // Initialize the slotted page layout in the raw page memory
    SlottedPage* slp = reinterpret_cast<SlottedPage*>(page->GetData());
    slp->Init(new_pid);

    first_page_id = new_pid;
    last_page_id  = new_pid;

    buffer_pool_manager->UnpinPage(new_pid, true);
}

// ─── Constructor: reopen an existing table ────────────────────────────────────
TableHeap::TableHeap(BufferPoolManager* bpm, page_id_t first_pid)
    : buffer_pool_manager(bpm), first_page_id(first_pid)
{
    last_page_id = FindLastPageId();
}

// Walk the page chain to find the tail page
page_id_t TableHeap::FindLastPageId() {
    page_id_t cur = first_page_id;
    while (true) {
        Page* page = buffer_pool_manager->FetchPage(cur);
        if (page == nullptr) break;

        SlottedPage* slp = reinterpret_cast<SlottedPage*>(page->GetData());
        page_id_t next = slp->Header()->next_page_id;
        buffer_pool_manager->UnpinPage(cur, false);

        if (next == INVALID_PAGE_ID) break;
        cur = next;
    }
    return cur;
}

// ─── InsertTuple ──────────────────────────────────────────────────────────────
bool TableHeap::InsertTuple(const Tuple& tuple, RID* rid, const Schema& schema) {
    // Try to insert into the last page first
    Page* page = buffer_pool_manager->FetchPage(last_page_id);
    if (page == nullptr) return false;

    SlottedPage* slp = reinterpret_cast<SlottedPage*>(page->GetData());
    slot_id_t out_slot;

    if (slp->InsertRecord(tuple.GetData(), tuple.GetLength(), &out_slot)) {
        *rid = RID(last_page_id, out_slot);
        buffer_pool_manager->UnpinPage(last_page_id, true);
        return true;
    }

    // Current last page is full — allocate a new page and append to the chain
    page_id_t new_pid;
    Page* new_page = buffer_pool_manager->NewPage(&new_pid);
    if (new_page == nullptr) {
        buffer_pool_manager->UnpinPage(last_page_id, false);
        return false;
    }

    // Initialize the new page
    SlottedPage* new_slp = reinterpret_cast<SlottedPage*>(new_page->GetData());
    new_slp->Init(new_pid);
    new_slp->Header()->next_page_id = INVALID_PAGE_ID;

    // Link the old last page to the new page
    slp->Header()->next_page_id = new_pid;
    buffer_pool_manager->UnpinPage(last_page_id, true);

    last_page_id = new_pid;

    if (new_slp->InsertRecord(tuple.GetData(), tuple.GetLength(), &out_slot)) {
        *rid = RID(new_pid, out_slot);
        buffer_pool_manager->UnpinPage(new_pid, true);
        return true;
    }

    buffer_pool_manager->UnpinPage(new_pid, false);
    return false;   // tuple is larger than an entire page — shouldn't happen in practice
}

// ─── GetTuple ─────────────────────────────────────────────────────────────────
bool TableHeap::GetTuple(const RID& rid, Tuple* tuple, const Schema& schema) {
    Page* page = buffer_pool_manager->FetchPage(rid.page_id);
    if (page == nullptr) return false;

    SlottedPage* slp = reinterpret_cast<SlottedPage*>(page->GetData());

    uint16_t len;
    const char* record = slp->GetRecord(rid.slot_id, &len);

    if (record != nullptr) {
        *tuple = Tuple(record, len);
        buffer_pool_manager->UnpinPage(rid.page_id, false);
        return true;
    }

    buffer_pool_manager->UnpinPage(rid.page_id, false);
    return false;
}