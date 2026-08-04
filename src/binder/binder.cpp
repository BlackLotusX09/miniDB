#include "parser/binder.h"
#include "parser/ast.h"
#include "catalog/catalog.h"
#include "catalog/schema.h"
#include <stdexcept>

// ---------------------------------------------------------------------------
// Bind — top-level dispatcher: downcasts Statement* and routes to the
// appropriate semantic-check method.
// ---------------------------------------------------------------------------
void Binder::Bind(Statement* stmt) {
    if (auto* s = dynamic_cast<SelectStatement*>(stmt)) {
        BindSelect(s);
    } else if (auto* s = dynamic_cast<InsertStatement*>(stmt)) {
        BindInsert(s);
    } else if (auto* s = dynamic_cast<DeleteStatement*>(stmt)) {
        BindDelete(s);
    }
    // CreateTable / CreateIndex need no binding (no columns to validate yet)
}

// ---------------------------------------------------------------------------
// BindSelect — validates table + columns, expands SELECT *
// ---------------------------------------------------------------------------
void Binder::BindSelect(SelectStatement* stmt) {
    TableInfo* tbl = catalog_->GetTable(stmt->tables);
    if (tbl == nullptr) {
        throw std::runtime_error("Table not found: " + stmt->tables);
    }

    // Resolve "*" to all column names from the schema
    if (stmt->column == std::vector<std::string>{"*"}) {
        stmt->column.clear();
        for (size_t i = 0; i < tbl->schema.GetColumnCount(); i++) {
            stmt->column.push_back(tbl->schema.GetColumn(i).name);
        }
    }

    // Validate every named column exists
    for (auto& col : stmt->column) {
        if (tbl->schema.GetColumnIndex(col) == -1) {
            throw std::runtime_error("Column '" + col + "' not found in table '" + stmt->tables + "'");
        }
    }

    // Validate WHERE predicate if present
    if (stmt->where != nullptr) {
        BindPredicate(stmt->where.get(), tbl->schema);
    }
}

// ---------------------------------------------------------------------------
// BindInsert — validates table exists, row count, and per-column types
// ---------------------------------------------------------------------------
void Binder::BindInsert(InsertStatement* stmt) {
    TableInfo* tbl = catalog_->GetTable(stmt->table);
    if (tbl == nullptr) {
        throw std::runtime_error("Table not found: " + stmt->table);
    }

    const Schema& schema = tbl->schema;
    for (auto& row : stmt->values) {
        if (row.size() != schema.GetColumnCount()) {
            throw std::runtime_error(
                "INSERT row has " + std::to_string(row.size()) +
                " values but table '" + stmt->table +
                "' has " + std::to_string(schema.GetColumnCount()) + " columns");
        }
        for (size_t i = 0; i < row.size(); i++) {
            if (row[i].GetType() != schema.GetColumn(i).type) {
                throw std::runtime_error(
                    "Type mismatch on column '" + schema.GetColumn(i).name +
                    "' in INSERT into '" + stmt->table + "'");
            }
        }
    }
}

// ---------------------------------------------------------------------------
// BindDelete — validates table exists, and WHERE predicate if present
// ---------------------------------------------------------------------------
void Binder::BindDelete(DeleteStatement* stmt) {
    TableInfo* tbl = catalog_->GetTable(stmt->table);
    if (tbl == nullptr) {
        throw std::runtime_error("Table not found: " + stmt->table);
    }
    if (stmt->where != nullptr) {
        BindPredicate(stmt->where.get(), tbl->schema);
    }
}

// ---------------------------------------------------------------------------
// BindPredicate — validates the WHERE column exists and literal type matches
// ---------------------------------------------------------------------------
void Binder::BindPredicate(Predicate* pred, const Schema& schema) {
    if (pred == nullptr) return;

    int idx = schema.GetColumnIndex(pred->col_name);
    if (idx == -1) {
        throw std::runtime_error("Column '" + pred->col_name + "' not found in WHERE clause");
    }

    const Column& col = schema.GetColumn(idx);
    if (pred->rhs.GetType() != col.type) {
        throw std::runtime_error(
            "Type mismatch in WHERE: column '" + pred->col_name +
            "' is " + (col.type == TypeId::INT ? "INT" :
                       col.type == TypeId::VARCHAR ? "VARCHAR" : "BOOL") +
            " but literal has a different type");
    }
}
