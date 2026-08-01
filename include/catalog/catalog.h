#pragma once

#include <iostream>
#include <string>
#include <unordered_map>
#include <memory>
#include <cstring>

#include "catalog/schema.h"
#include "catalog/tuple.h"
#include "storage/page.h"
#include "storage/TableHeap.h"
#include "storage/b_tree.h"
#include "storage/BufferPool.h"

struct TableInfo {
    std::string          name;
    Schema               schema;
    page_id_t            first_page_id;
    page_id_t            index_root_page_id{INVALID_PAGE_ID};
    std::unique_ptr<TableHeap>  heap;
    std::unique_ptr<BPlusTree>  index;  // nullptr if no index
};

class Catalog {
public:
    explicit Catalog(BufferPoolManager* bpm);  // loads from disk on construction

    void        CreateTable(const std::string& name, const Schema& schema);
    TableInfo*  GetTable(const std::string& name);
    void        SaveToDisk();   // serialize catalog → page 0

private:
    void        LoadFromDisk(); // deserialize page 0 → tables_

    std::unordered_map<std::string, TableInfo> tables_;
    BufferPoolManager* bpm_;
};