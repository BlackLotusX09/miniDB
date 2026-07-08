#pragma once
#include<vector>
#include"BufferPool.h"
#include"schema.h"
#include"tuple.h"
#include"rid.h"
class TableHeap{
private:
    BufferPoolManager* buffer_pool_manager;
    page_id_t first_page_id;
    page_id_t last_page_id;
public:
    TableHeap(BufferPoolManager* buffer_pool_manager, page_id_t first_page_id);
    bool InsertTuple(const Tuple& tuple,RID* rid, const Schema& schema);
    bool GetTuple(const RID& rid, Tuple* tuple, const Schema& schema);
};