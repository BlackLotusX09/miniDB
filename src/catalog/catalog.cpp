#include "catalog/catalog.h"
#include <cstring>

// ─── Binary layout of page 0 ────────────────────────────────────────────────
//
//  [ uint16_t  num_tables ]                         2 bytes
//
//  For each table:
//    [ uint8_t   name_len        ]                  1 byte
//    [ char      name[name_len]  ]                  name_len bytes
//    [ page_id_t first_page_id   ]                  4 bytes
//    [ page_id_t index_root_id   ]                  4 bytes  (INVALID_PAGE_ID = no index)
//    [ uint8_t   col_count       ]                  1 byte
//    For each column:
//      [ uint8_t   type          ]                  1 byte  (0=INT, 1=BOOL, 2=VARCHAR)
//      [ uint32_t  max_length    ]                  4 bytes (meaningful for VARCHAR)
//      [ uint8_t   col_name_len  ]                  1 byte
//      [ char      col_name[]    ]                  col_name_len bytes
//
// ────────────────────────────────────────────────────────────────────────────

// ─── Constructor ─────────────────────────────────────────────────────────────
Catalog::Catalog(BufferPoolManager* bpm) : bpm_(bpm) {
    LoadFromDisk();
}

// ─── LoadFromDisk ─────────────────────────────────────────────────────────────
void Catalog::LoadFromDisk() {
    Page* catalog_page = bpm_->FetchPage(0);
    if (catalog_page == nullptr) return;

    const char* data = catalog_page->GetData();
    uint16_t offset = 0;

    // Read number of tables (0 on fresh DB since page is zero-initialised)
    uint16_t num_tables = 0;
    std::memcpy(&num_tables, data + offset, sizeof(uint16_t));
    offset += sizeof(uint16_t);

    for (uint16_t i = 0; i < num_tables; i++) {
        // ── table name ──────────────────────────────────────────────────────
        uint8_t name_len = 0;
        std::memcpy(&name_len, data + offset, sizeof(uint8_t));
        offset += sizeof(uint8_t);

        std::string name(data + offset, name_len);
        offset += name_len;

        // ── first_page_id ───────────────────────────────────────────────────
        page_id_t first_page_id = INVALID_PAGE_ID;
        std::memcpy(&first_page_id, data + offset, sizeof(page_id_t));
        offset += sizeof(page_id_t);

        // ── index_root_page_id ──────────────────────────────────────────────
        page_id_t index_root_page_id = INVALID_PAGE_ID;
        std::memcpy(&index_root_page_id, data + offset, sizeof(page_id_t));
        offset += sizeof(page_id_t);

        // ── schema (columns) ────────────────────────────────────────────────
        uint8_t col_count = 0;
        std::memcpy(&col_count, data + offset, sizeof(uint8_t));
        offset += sizeof(uint8_t);

        std::vector<Column> columns;
        columns.reserve(col_count);

        for (uint8_t c = 0; c < col_count; c++) {
            // type
            uint8_t raw_type = 0;
            std::memcpy(&raw_type, data + offset, sizeof(uint8_t));
            offset += sizeof(uint8_t);
            TypeId type = static_cast<TypeId>(raw_type);

            // max_length
            uint32_t max_length = 0;
            std::memcpy(&max_length, data + offset, sizeof(uint32_t));
            offset += sizeof(uint32_t);

            // column name
            uint8_t col_name_len = 0;
            std::memcpy(&col_name_len, data + offset, sizeof(uint8_t));
            offset += sizeof(uint8_t);

            std::string col_name(data + offset, col_name_len);
            offset += col_name_len;

            columns.push_back(Column{col_name, type, max_length});
        }

        // ── reconstruct TableInfo ────────────────────────────────────────────
        TableInfo info;
        info.name               = name;
        info.schema             = Schema(std::move(columns));
        info.first_page_id      = first_page_id;
        info.index_root_page_id = index_root_page_id;
        info.heap               = std::make_unique<TableHeap>(bpm_, first_page_id);
        info.index              = nullptr;  // B-tree wired in Phase 3/4

        tables_[name] = std::move(info);
    }

    bpm_->UnpinPage(0, false);  // catalog page was only read
}

// ─── SaveToDisk ──────────────────────────────────────────────────────────────
void Catalog::SaveToDisk() {
    Page* catalog_page = bpm_->FetchPage(0);
    if (catalog_page == nullptr) return;

    char* data = catalog_page->GetData();
    std::memset(data, 0, PAGE_SIZE);   // start clean
    uint16_t offset = 0;

    // number of tables
    uint16_t num_tables = static_cast<uint16_t>(tables_.size());
    std::memcpy(data + offset, &num_tables, sizeof(uint16_t));
    offset += sizeof(uint16_t);

    for (const auto& [name, info] : tables_) {
        // ── table name ──────────────────────────────────────────────────────
        uint8_t name_len = static_cast<uint8_t>(name.size());
        std::memcpy(data + offset, &name_len, sizeof(uint8_t));
        offset += sizeof(uint8_t);

        std::memcpy(data + offset, name.data(), name_len);
        offset += name_len;

        // ── first_page_id ───────────────────────────────────────────────────
        std::memcpy(data + offset, &info.first_page_id, sizeof(page_id_t));
        offset += sizeof(page_id_t);

        // ── index_root_page_id ──────────────────────────────────────────────
        std::memcpy(data + offset, &info.index_root_page_id, sizeof(page_id_t));
        offset += sizeof(page_id_t);

        // ── schema ──────────────────────────────────────────────────────────
        const auto& cols = info.schema.GetColumns();
        uint8_t col_count = static_cast<uint8_t>(cols.size());
        std::memcpy(data + offset, &col_count, sizeof(uint8_t));
        offset += sizeof(uint8_t);

        for (const Column& col : cols) {
            // type
            uint8_t raw_type = static_cast<uint8_t>(col.type);
            std::memcpy(data + offset, &raw_type, sizeof(uint8_t));
            offset += sizeof(uint8_t);

            // max_length
            std::memcpy(data + offset, &col.length, sizeof(uint32_t));
            offset += sizeof(uint32_t);

            // column name
            uint8_t col_name_len = static_cast<uint8_t>(col.name.size());
            std::memcpy(data + offset, &col_name_len, sizeof(uint8_t));
            offset += sizeof(uint8_t);

            std::memcpy(data + offset, col.name.data(), col_name_len);
            offset += col_name_len;
        }
    }

    bpm_->UnpinPage(0, true);   // mark dirty so BPM flushes it
    bpm_->flushAllPages();      // guarantee persistence before returning
}

// ─── CreateTable ─────────────────────────────────────────────────────────────
void Catalog::CreateTable(const std::string& name, const Schema& schema) {
    if (tables_.count(name)) return;   // duplicate — no-op

    // Allocate a brand-new heap (the 1-arg constructor calls NewPage internally)
    auto heap = std::make_unique<TableHeap>(bpm_);
    page_id_t first_pid = heap->GetFirstPageId();

    TableInfo info;
    info.name               = name;
    info.schema             = schema;
    info.first_page_id      = first_pid;
    info.index_root_page_id = INVALID_PAGE_ID;
    info.heap               = std::move(heap);
    info.index              = nullptr;

    tables_[name] = std::move(info);
    SaveToDisk();   // persist immediately — crash-safe
}

// ─── GetTable ─────────────────────────────────────────────────────────────────
TableInfo* Catalog::GetTable(const std::string& name) {
    auto it = tables_.find(name);
    if (it == tables_.end()) return nullptr;
    return &it->second;
}