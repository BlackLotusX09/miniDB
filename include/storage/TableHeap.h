#pragma once
#include <vector>
#include "storage/BufferPool.h"
#include "storage/schema.h"
#include "storage/tuple.h"
#include "storage/rid.h"
#include "storage/TableIterator.h"

class TableHeap {
private:
    BufferPoolManager* buffer_pool_manager;
    page_id_t first_page_id;
    page_id_t last_page_id;

    // Walk the page chain to find the last page
    page_id_t FindLastPageId();

public:
    // Create a brand-new table (allocates the first page automatically)
    explicit TableHeap(BufferPoolManager* bpm);

    // Reopen an existing table given the known first page id
    TableHeap(BufferPoolManager* bpm, page_id_t first_page_id);

    bool InsertTuple(const Tuple& tuple, RID* rid, const Schema& schema);
    bool GetTuple(const RID& rid, Tuple* tuple, const Schema& schema);

    page_id_t GetFirstPageId() const { return first_page_id; }

    TableIterator Begin() { return TableIterator(buffer_pool_manager, first_page_id); }
};