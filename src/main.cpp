#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "storage/DiskManager.h"
#include "storage/BufferPool.h"
#include "catalog/catalog.h"
#include "catalog/tuple.h"
#include "parser/lexer.h"
#include "parser/parser.h"
#include "parser/binder.h"

int main() {
    DiskManager dm("minidb.db");
    BufferPoolManager bpm(&dm);
    Catalog catalog(&bpm);   // loads existing tables from disk (page 0)

    std::cout << "miniDB ready. Type SQL and end with ';'. Type .exit to quit.\n";

    std::string line;
    std::string sql;

    while (true) {
        std::cout << (sql.empty() ? "minidb> " : "     -> ");
        if (!std::getline(std::cin, line)) break;   // EOF / Ctrl-D

        if (line == ".exit" || line == "exit" || line == "quit") break;
        if (line.empty()) continue;

        sql += " " + line;

        // Only process once we have a semicolon
        if (sql.find(';') == std::string::npos) continue;

        try {
            Lexer  lexer(sql);
            Parser parser(lexer);
            auto   stmt = parser.Parse();

            Binder binder(&catalog);
            binder.Bind(stmt.get());   // semantic validation

            std::cout << "[ok] Statement parsed and bound successfully.\n";

            // TODO: pass stmt to executor
        } catch (const std::exception& e) {
            std::cerr << "[error] " << e.what() << "\n";
        }

        sql.clear();
        catalog.SaveToDisk();
    }

    catalog.SaveToDisk();
    bpm.flushAllPages();
    return 0;
}