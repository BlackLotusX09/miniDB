#pragma once

#include "executor.h"
#include "catalog/catalog.h"
#include "parser/ast.h"
#include "catalog/tuple.h"
#include "catalog/schema.h"
#include "catalog/catalog.h"

struct ResultSet{
    vector<Tuple> tuples;
    Schema schema;
};
class ExecutorEngine{
public:
    ExecutorEngine(Catalog* catalog, BufferPoolManager* bpm_);
    void Execute(Statement* stmt, ResultSet* result);
private:
    std::unique_ptr<Executor> BuildExecutor(Statement* stmt);
    Catalog* catalog_;
    BufferPoolManager* bpm_;
};