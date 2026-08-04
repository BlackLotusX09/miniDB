// Scratch: debug binder_test failure
#include <iostream>
#include <stdexcept>
#include "storage/DiskManager.h"
#include "storage/BufferPool.h"
#include "catalog/catalog.h"
#include "catalog/schema.h"
#include "parser/lexer.h"
#include "parser/parser.h"
#include "parser/binder.h"

int main() {
    DiskManager dm("binder_debug.db");
    BufferPoolManager bpm(&dm);
    Catalog catalog(&bpm);

    Schema schema({
        Column{"id",     TypeId::INT,     0},
        Column{"name",   TypeId::VARCHAR, 64},
        Column{"active", TypeId::BOOL,    0},
    });
    catalog.CreateTable("users", schema);

    auto test = [&](const std::string& sql) {
        std::cout << "SQL: " << sql << "\n";
        try {
            Lexer lex(sql);
            Parser parser(lex);
            auto stmt = parser.Parse();
            Binder binder(&catalog);
            binder.Bind(stmt.get());
            std::cout << "  -> PASS\n";
        } catch (const std::exception& e) {
            std::cout << "  -> FAIL: " << e.what() << "\n";
        }
    };

    test("SELECT id, name FROM users;");
    test("SELECT * FROM users;");
    test("SELECT ghost FROM users;");
    test("SELECT id FROM ghost_table;");
    test("SELECT id FROM users WHERE id = 1;");
    return 0;
}
