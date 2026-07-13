#pragma once
#include "storage/BufferPool.h"
#include "storage/tuple.h"

using namespace std;

class TableIterator {
private:
    BufferPoolManager* bpm;
    page_id_t page_id;
    slot_id_t slot_id;
    Page* current_page_ptr;
public:
    TableIterator(BufferPoolManager *bpm, page_id_t start_page);
    bool Move(Tuple* tuple);
};