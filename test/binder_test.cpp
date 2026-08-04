#include <cassert>
#include <iostream>
#include <stdexcept>

#include "storage/DiskManager.h"
#include "storage/BufferPool.h"
#include "catalog/catalog.h"
#include "catalog/schema.h"
#include "parser/lexer.h"
#include "parser/parser.h"
#include "parser/binder.h"

// Helper: parse + bind a SQL string against a catalog.
// Returns true if it passes, false if binder threw.
static bool bindOk(const std::string& sql, Catalog& catalog) {
    try {
        Lexer  lex(sql);
        Parser parser(lex);
        auto   stmt = parser.Parse();
        Binder binder(&catalog);
        binder.Bind(stmt.get());
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

int main() {
    // In-memory setup
    DiskManager dm("binder_test.db");
    BufferPoolManager bpm(&dm);
    Catalog catalog(&bpm);

    // Create the test table
    Schema schema({
        Column{"id",     TypeId::INT,     0},
        Column{"name",   TypeId::VARCHAR, 64},
        Column{"active", TypeId::BOOL,    0},
    });
    catalog.CreateTable("users", schema);

    // ── SELECT tests ───────────────────────────────────────────────────────
    assert( bindOk("SELECT id, name FROM users;",    catalog) && "SELECT valid cols");
    assert( bindOk("SELECT * FROM users;",           catalog) && "SELECT *");
    assert(!bindOk("SELECT ghost FROM users;",       catalog) && "SELECT bad col");
    assert(!bindOk("SELECT id FROM ghost_table;",    catalog) && "SELECT bad table");

    // ── WHERE tests ────────────────────────────────────────────────────────
    assert( bindOk("SELECT id FROM users WHERE id = 1;",      catalog) && "WHERE INT=INT");
    assert( bindOk("SELECT id FROM users WHERE name = 'Alice';", catalog) && "WHERE VARCHAR=VARCHAR");
    assert(!bindOk("SELECT id FROM users WHERE id = 'Alice';", catalog) && "WHERE INT=VARCHAR type error");
    assert(!bindOk("SELECT id FROM users WHERE ghost = 1;",   catalog) && "WHERE bad col");

    // ── INSERT tests ───────────────────────────────────────────────────────
    assert( bindOk("INSERT INTO users VALUES (1, 'Alice', true);",    catalog) && "INSERT valid");
    // Wrong count
    assert(!bindOk("INSERT INTO users VALUES (1, 'Alice');",          catalog) && "INSERT wrong count");
    // Wrong table
    assert(!bindOk("INSERT INTO ghost VALUES (1);",                    catalog) && "INSERT bad table");

    // ── DELETE tests ───────────────────────────────────────────────────────
    assert( bindOk("DELETE FROM users;",                              catalog) && "DELETE no WHERE");
    assert( bindOk("DELETE FROM users WHERE id = 42;",               catalog) && "DELETE with WHERE");
    assert(!bindOk("DELETE FROM ghost;",                              catalog) && "DELETE bad table");
    assert(!bindOk("DELETE FROM users WHERE ghost = 1;",             catalog) && "DELETE bad WHERE col");

    std::cout << "All binder tests passed!\n";
    return 0;
}
