#pragma once
#include "ast.h"
#include "catalog/catalog.h"

#include "catalog/schema.h"

class Binder{
    Catalog* catalog_;
public:
    explicit Binder(Catalog* catalog) : catalog_(catalog){};
    void Bind(Statement* stmt);
private:
    void BindSelect(SelectStatement* stmt);
    void BindInsert(InsertStatement* stmt);
    void BindDelete(DeleteStatement* stmt);
    void BindPredicate(Predicate* pred, const Schema& schema);
};
