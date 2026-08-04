#pragma once
#include "catalog/schema.h"
#include "token.h"
#include <memory>
#include <vector>

enum class Op {EQ, NEQ, LT, GT, LTE, GTE};

// Literal: a constant value that appears in SQL text (e.g., 42, 'Alice').
// Named "Literal" to avoid clashing with catalog::Value (the runtime Value class).
struct Literal {
    TypeId type;
    int32_t int_value = 0;
    std::string str_val;
    bool bool_val = false;
    // Uniform accessor used by the binder for type-checking
    TypeId GetType() const { return type; }
};

// Base AST node
struct Statement{
    virtual ~Statement()=default;
};
 
// Predicate: one comparison like "age > 18" or "id = 5"
struct Predicate{
    std::string col_name;
    Op op;
    Literal rhs;
    Predicate(std::string colName, Op operation, Literal right_val): col_name(colName), op(operation), rhs(right_val){}
};

struct JoinClause{
    std::string right_table;
    std::string right_alias;
    std::string left_col;
    std::string right_col;
};

struct SelectStatement : Statement{
    std::vector<std::string> column;
    std::string tables;
    std::string alias;
    std::unique_ptr<Predicate> where;
    std::vector<JoinClause> joins;
};

struct InsertStatement : Statement{
    std::string table;
    std::vector<std::vector<Literal>> values;

    InsertStatement(std::string t, std::vector<std::vector<Literal>> v)
        : table(std::move(t)), values(std::move(v)) {}
};

struct DeleteStatement : Statement{
    std::string table;
    std::unique_ptr<Predicate> where;

    DeleteStatement(std::string table_name, std::unique_ptr<Predicate> where): table(std::move(table_name)), where(std::move(where)) {}
};

// Base node for all CREATE statements
struct Create : Statement {};

struct CreateTable : Create{
    std::string table;
    std::vector<Column> columns;

    CreateTable(std::string t, std::vector<Column> cols)
        : table(std::move(t)), columns(std::move(cols)) {}
};

struct CreateIndex : Create{
    std::string table;
    std::string column;
}; 
