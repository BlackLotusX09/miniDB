#include <iostream>
#include <vector>
#include "storage/DiskManager.h"
#include "storage/BufferPool.h"
#include "catalog/catalog.h"
#include "catalog/tuple.h"
#include "catalog/value.h"

int main() {
    DiskManager dm("minidb.db");
    BufferPoolManager bpm(&dm);
    Catalog catalog(&bpm);   // loads from disk (empty on first run)

    // Create the "users" table if it doesn't exist yet
    if (catalog.GetTable("users") == nullptr) {
        Schema users_schema({
            Column{"id",     TypeId::INT,     0},
            Column{"name",   TypeId::VARCHAR, 64},
            Column{"active", TypeId::BOOL,    0},
        });
        catalog.CreateTable("users", users_schema);
        std::cout << "[catalog] Created table 'users'\n";
    } else {
        std::cout << "[catalog] Loaded existing table 'users'\n";
    }

    TableInfo* info = catalog.GetTable("users");
    std::cout << "[catalog] 'users' first_page_id = " << info->first_page_id << "\n";
    std::cout << "[catalog] 'users' column count  = " << info->schema.GetColumnCount() << "\n";

    // Insert a sample tuple
    std::vector<Value> row = {
        Value(static_cast<int32_t>(1)),
        Value(std::string("Alice")),
        Value(true),
    };
    Tuple t(row, info->schema);
    RID rid;
    if (info->heap->InsertTuple(t, &rid, info->schema)) {
        std::cout << "[catalog] Inserted tuple at page=" << rid.page_id
                  << " slot=" << rid.slot_id << "\n";
    }

    bpm.flushAllPages();
    return 0;
}