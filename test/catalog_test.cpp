// catalog_test.cpp — 6 test cases for the Catalog class
// Build via CMake target: catalog_test
// Usage: ./catalog_test   (prints PASS or FAIL for each case)

#include "catalog/catalog.h"
#include "storage/DiskManager.h"
#include "storage/BufferPool.h"

#include <cassert>
#include <cstdio>
#include <iostream>
#include <string>

// ─── helpers ─────────────────────────────────────────────────────────────────

static Schema MakeUsersSchema() {
    return Schema({
        Column{"id",   TypeId::INT,     0},
        Column{"name", TypeId::VARCHAR, 64},
        Column{"active", TypeId::BOOL,  0},
    });
}

static void RemoveTestFiles(const std::string& base) {
    std::remove(base.c_str());
    std::remove((base + ".meta").c_str());
}

static void Pass(int n) {
    std::cout << "[PASS] Test " << n << "\n";
}
static void Fail(int n, const std::string& msg) {
    std::cerr << "[FAIL] Test " << n << ": " << msg << "\n";
}

// ─── Test 1: CreateTable then GetTable returns correct TableInfo ───────────────
static void Test1() {
    const std::string db = "test_catalog_t1.db";
    RemoveTestFiles(db);

    DiskManager dm(db);
    BufferPoolManager bpm(&dm);
    Catalog cat(&bpm);

    cat.CreateTable("users", MakeUsersSchema());
    TableInfo* info = cat.GetTable("users");

    if (info == nullptr)               { Fail(1, "GetTable returned nullptr"); return; }
    if (info->name != "users")         { Fail(1, "name mismatch"); return; }
    if (info->first_page_id < 1)       { Fail(1, "first_page_id should be >= 1 (page 0 reserved)"); return; }
    if (info->schema.GetColumnCount() != 3) { Fail(1, "schema column count mismatch"); return; }

    bpm.flushAllPages();
    RemoveTestFiles(db);
    Pass(1);
}

// ─── Test 2: GetTable on unknown name returns nullptr ─────────────────────────
static void Test2() {
    const std::string db = "test_catalog_t2.db";
    RemoveTestFiles(db);

    DiskManager dm(db);
    BufferPoolManager bpm(&dm);
    Catalog cat(&bpm);

    TableInfo* info = cat.GetTable("nonexistent");
    if (info != nullptr) { Fail(2, "expected nullptr for unknown table"); return; }

    bpm.flushAllPages();
    RemoveTestFiles(db);
    Pass(2);
}

// ─── Test 3: Duplicate CreateTable is a no-op ─────────────────────────────────
static void Test3() {
    const std::string db = "test_catalog_t3.db";
    RemoveTestFiles(db);

    DiskManager dm(db);
    BufferPoolManager bpm(&dm);
    Catalog cat(&bpm);

    cat.CreateTable("items", MakeUsersSchema());
    TableInfo* first = cat.GetTable("items");
    page_id_t pid1 = first->first_page_id;

    // Second call should be ignored
    cat.CreateTable("items", MakeUsersSchema());
    TableInfo* second = cat.GetTable("items");

    if (second == nullptr)              { Fail(3, "table disappeared after duplicate create"); return; }
    if (second->first_page_id != pid1)  { Fail(3, "first_page_id changed on duplicate create"); return; }

    bpm.flushAllPages();
    RemoveTestFiles(db);
    Pass(3);
}

// ─── Test 4: Persistence round-trip ──────────────────────────────────────────
// Create a table, destroy everything, re-open, verify the table is recovered.
static void Test4() {
    const std::string db = "test_catalog_t4.db";
    RemoveTestFiles(db);

    page_id_t saved_first_pid = INVALID_PAGE_ID;

    // --- first session ---
    {
        DiskManager dm(db);
        BufferPoolManager bpm(&dm);
        Catalog cat(&bpm);

        cat.CreateTable("orders", MakeUsersSchema());
        TableInfo* info = cat.GetTable("orders");
        saved_first_pid = info->first_page_id;
        bpm.flushAllPages();
    }

    // --- second session (fresh objects, same file) ---
    {
        DiskManager dm(db);
        BufferPoolManager bpm(&dm);
        Catalog cat(&bpm);   // should LoadFromDisk in constructor

        TableInfo* info = cat.GetTable("orders");
        if (info == nullptr)                         { Fail(4, "table not found after restart"); return; }
        if (info->name != "orders")                  { Fail(4, "name mismatch after restart"); return; }
        if (info->first_page_id != saved_first_pid)  { Fail(4, "first_page_id changed across restart"); return; }
        if (info->heap == nullptr)                   { Fail(4, "heap not reconstructed"); return; }

        bpm.flushAllPages();
    }

    RemoveTestFiles(db);
    Pass(4);
}

// ─── Test 5: Schema columns survive serialization / deserialization ───────────
static void Test5() {
    const std::string db = "test_catalog_t5.db";
    RemoveTestFiles(db);

    // --- first session ---
    {
        DiskManager dm(db);
        BufferPoolManager bpm(&dm);
        Catalog cat(&bpm);
        cat.CreateTable("products", MakeUsersSchema());
        bpm.flushAllPages();
    }

    // --- second session ---
    {
        DiskManager dm(db);
        BufferPoolManager bpm(&dm);
        Catalog cat(&bpm);

        TableInfo* info = cat.GetTable("products");
        if (info == nullptr) { Fail(5, "table not found"); return; }

        const auto& cols = info->schema.GetColumns();
        if (cols.size() != 3)             { Fail(5, "column count wrong"); return; }
        if (cols[0].name != "id")         { Fail(5, "col[0] name wrong"); return; }
        if (cols[0].type != TypeId::INT)  { Fail(5, "col[0] type wrong"); return; }
        if (cols[1].name != "name")       { Fail(5, "col[1] name wrong"); return; }
        if (cols[1].type != TypeId::VARCHAR) { Fail(5, "col[1] type wrong"); return; }
        if (cols[1].length != 64)         { Fail(5, "col[1] length wrong"); return; }
        if (cols[2].name != "active")     { Fail(5, "col[2] name wrong"); return; }
        if (cols[2].type != TypeId::BOOL) { Fail(5, "col[2] type wrong"); return; }

        bpm.flushAllPages();
    }

    RemoveTestFiles(db);
    Pass(5);
}

// ─── Test 6: Multiple tables persist and are all recovered ────────────────────
static void Test6() {
    const std::string db = "test_catalog_t6.db";
    RemoveTestFiles(db);

    Schema s1({ Column{"a", TypeId::INT, 0} });
    Schema s2({ Column{"b", TypeId::VARCHAR, 32}, Column{"c", TypeId::BOOL, 0} });
    Schema s3({ Column{"x", TypeId::INT, 0}, Column{"y", TypeId::INT, 0}, Column{"z", TypeId::INT, 0} });

    // --- first session ---
    {
        DiskManager dm(db);
        BufferPoolManager bpm(&dm);
        Catalog cat(&bpm);
        cat.CreateTable("alpha",   s1);
        cat.CreateTable("beta",    s2);
        cat.CreateTable("gamma",   s3);
        bpm.flushAllPages();
    }

    // --- second session ---
    {
        DiskManager dm(db);
        BufferPoolManager bpm(&dm);
        Catalog cat(&bpm);

        if (cat.GetTable("alpha") == nullptr)  { Fail(6, "alpha not found"); return; }
        if (cat.GetTable("beta") == nullptr)   { Fail(6, "beta not found"); return; }
        if (cat.GetTable("gamma") == nullptr)  { Fail(6, "gamma not found"); return; }

        if (cat.GetTable("alpha")->schema.GetColumnCount() != 1) { Fail(6, "alpha col count wrong"); return; }
        if (cat.GetTable("beta")->schema.GetColumnCount()  != 2) { Fail(6, "beta col count wrong"); return; }
        if (cat.GetTable("gamma")->schema.GetColumnCount() != 3) { Fail(6, "gamma col count wrong"); return; }

        bpm.flushAllPages();
    }

    RemoveTestFiles(db);
    Pass(6);
}

// ─── main ─────────────────────────────────────────────────────────────────────
int main() {
    Test1();
    Test2();
    Test3();
    Test4();
    Test5();
    Test6();
    return 0;
}
