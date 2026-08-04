#pragma once
#include <memory>   // unique_ptr, make_unique
#include <utility>  // move (if needed)
#include "catalog/tuple.h"
#include "catalog/schema.h"
#include "parser/ast.h"
#include "storage/rid.h"
#include "catalog/catalog.h"
#include "storage/TableIterator.h"
#include "storage/BufferPool.h"
using namespace std;

class Executor{
public:
    virtual void Init() = 0;
    virtual bool Next(Tuple* out, RID* RID) = 0;
    virtual void Close() = 0;
    virtual const Schema* OutputSchema() const = 0;
    virtual ~Executor() = default;
};

//Sequence Scan Decleration
class SeqScanExecutor : public Executor{
public:
    SeqScanExecutor(TableInfo* table, BufferPoolManager* bpm);
    void Init() override;
    bool Next(Tuple* out, RID* rid) override;
    void Close() override;
    const Schema* OutputSchema() const override;
private:
    TableInfo* table_;
    BufferPoolManager* bpm_;
    TableIterator iter_;
};

//Filter Executor Decleration
class FilterExecutor : public Executor{
public:
    FilterExecutor(std::unique_ptr<Executor> child, std::unique_ptr<Predicate> predicate);
    void Init() override;
    bool Next(Tuple* out, RID* rid) override;
    void Close() override;
    const Schema* OutputSchema() const override;
    bool Evaluate(const Tuple& t);
private:
    std::unique_ptr<Executor> child_executor_;
    std::unique_ptr<Predicate> predicate_;

};
