#pragma once
#include "storage/BufferPool.h"
#include "storage/rid.h"
#include "catalog/tuple.h"

class TableIterator {
private:
    BufferPoolManager* bpm_;
    page_id_t          page_id_;    // INVALID_PAGE_ID means "end"
    slot_id_t          slot_id_;
    Page*              current_page_;

    // Skips deleted/tombstoned slots until a valid one is found,
    // or sets page_id_ = INVALID_PAGE_ID if the table is exhausted.
    void AdvanceToValid();

public:
    // Normal constructor — points to the first valid record starting at start_page
    TableIterator(BufferPoolManager* bpm, page_id_t start_page);

    // End sentinel constructor — used by TableHeap::End()
    // page_id = INVALID_PAGE_ID marks the "past-the-end" state
    TableIterator();

    // Destructor — unpins the current page if one is held
    ~TableIterator();

    // ── Read the current position ──────────────────────────────────────
    const char* GetData()   const;   // raw bytes of the current slot
    uint16_t    GetLength() const;   // byte length of the current slot
    RID         GetRID()    const;   // {page_id_, slot_id_}  (before advance)

    // ── Advance ────────────────────────────────────────────────────────
    TableIterator& operator++();     // prefix ++it  (advance to next valid slot)

    // ── Comparison ─────────────────────────────────────────────────────
    bool operator==(const TableIterator& other) const;
    bool operator!=(const TableIterator& other) const;
};
