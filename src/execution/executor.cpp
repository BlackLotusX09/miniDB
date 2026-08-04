#include "execution/executor.h"
#include "catalog/schema.h"
//SeqScan implementation
SeqScanExecutor::SeqScanExecutor(TableInfo* table,BufferPoolManager* bpm){
    table_ = table;
    bpm_=bpm;
}

void SeqScanExecutor::Init(){
    iter_ = table_->heap.get()->Begin();
}
bool SeqScanExecutor::Next(Tuple* out, RID* rid){
    if (iter_ == table_->heap.get()->End()) return false;
    *out = Tuple(iter_.GetData(), iter_.GetLength());
    if (rid) *rid = iter_.GetRID();
    ++iter_;
    return true;
}
const Schema* SeqScanExecutor::OutputSchema() const{
    return &(table_->schema);
}
void SeqScanExecutor::Close(){
    iter_ = table_->heap.get()->End();
}

//Filter Implementation
FilterExecutor::FilterExecutor(std::unique_ptr<Executor> child, std::unique_ptr<Predicate> predicate) : 
child_executor_(std::move(child)), 
predicate_(std::move(predicate)) {}

void FilterExecutor::Init() {
    child_executor_->Init();
}

bool FilterExecutor::Next(Tuple* out, RID* rid) {
    while (child_executor_->Next(out, rid)) {
        if (Evaluate(*out)) {
            return true;
        }
    }
    return false;
}

void FilterExecutor::Close() {
    child_executor_->Close();
}

const Schema* FilterExecutor::OutputSchema() const {
    return child_executor_->OutputSchema();
}

bool FilterExecutor::Evaluate(const Tuple& t){
    int col_idx = child_executor_->OutputSchema()->GetColumnIndex(predicate_->col_name);
    if (col_idx == -1) return false;
    Value value = t.GetValue(child_executor_->OutputSchema(), col_idx);
    if (value.GetType() != predicate_->rhs.type) return false;

    if (value.GetType() == TypeId::INT) {
        int l = value.AsInt();
        int r = predicate_->rhs.int_value;
        switch (predicate_->op) {
            case Op::EQ:  return l == r;
            case Op::NEQ: return l != r;
            case Op::LT:  return l <  r;
            case Op::GT:  return l >  r;
            case Op::LTE: return l <= r;
            case Op::GTE: return l >= r;
            default:      return false;
        }
    }
    if (value.GetType() == TypeId::VARCHAR) {
        const std::string& l = value.AsString();
        const std::string& r = predicate_->rhs.str_val;
        switch (predicate_->op) {
            case Op::EQ:  return l == r;
            case Op::NEQ: return l != r;
            case Op::LT:  return l <  r;
            case Op::LTE: return l <= r;
            case Op::GT:  return l >  r;
            case Op::GTE: return l >= r;
            default:      return false;
        }
    }
    return false;
}

//Projection Executor
ProjectionExecutor::ProjectionExecutor(std::unique_ptr<Executor> child, std::vector<std::string> col_names)
    : child_executor_(std::move(child)), col_names_(std::move(col_names)) {
    // Build the output schema once, at construction time
    const Schema* child_schema = child_executor_->OutputSchema();
    std::vector<Column> cols;
    for (const auto& name : col_names_) {
        int idx = child_schema->GetColumnIndex(name);
        if (idx != -1) {
            cols.push_back(child_schema->GetColumn(static_cast<size_t>(idx)));
        }
    }
    output_schema_ = Schema(std::move(cols));
}

void ProjectionExecutor::Init() {
    child_executor_->Init();
}

bool ProjectionExecutor::Next(Tuple* out, RID* rid) {
    Tuple child_tuple;
    if (!child_executor_->Next(&child_tuple, rid)) return false;

    const Schema* child_schema = child_executor_->OutputSchema();
    std::vector<Value> projected;
    projected.reserve(col_names_.size());
    for (const auto& name : col_names_) {
        int idx = child_schema->GetColumnIndex(name);
        projected.push_back(child_tuple.GetValue(child_schema, static_cast<size_t>(idx)));
    }
    *out = Tuple(projected, output_schema_);
    return true;
}

void ProjectionExecutor::Close() {
    child_executor_->Close();
}

const Schema* ProjectionExecutor::OutputSchema() const {
    return &output_schema_;
}


//Insert Executor
InsertExecutor::InsertExecutor(TableInfo* table_info, std::vector<Value> values)
    : table_info_(table_info), values_(std::move(values)), done_(false) {
    // Output schema: single INT column reporting rows affected
    output_schema_ = Schema({Column{"rows_affected", TypeId::INT, 4}});
}

void InsertExecutor::Init() { done_ = false; }
void InsertExecutor::Close() {}

const Schema* InsertExecutor::OutputSchema() const { return &output_schema_; }

bool InsertExecutor::Next(Tuple* out, RID* rid_out) {
    if (done_) return false;
    done_ = true;

    // 1. Serialize values → Tuple
    Tuple buf(values_, table_info_->schema);

    // 2. Insert into heap
    RID rid;
    if (!table_info_->heap->InsertTuple(buf, &rid, table_info_->schema)) return false;

    // 3. Update index if one exists (index is on column 0 by convention)
    if (table_info_->index) {
        Value key_val = buf.GetValue(&table_info_->schema, 0);
        table_info_->index->insert(key_val.AsInt(), rid);
    }

    // 4. Emit "1 row inserted" count tuple
    *out = Tuple({Value(int32_t{1})}, output_schema_);
    return true;
}

//Delete Executor
DeleteExecutor::DeleteExecutor(TableInfo* table_info, std::unique_ptr<Executor> child)
    : table_info_(table_info), child_executor_(std::move(child)) {
    output_schema_ = Schema({Column{"rows_affected", TypeId::INT, 4}});
}

void DeleteExecutor::Init() { child_executor_->Init(); }
void DeleteExecutor::Close() { child_executor_->Close(); }

const Schema* DeleteExecutor::OutputSchema() const { return &output_schema_; }

bool DeleteExecutor::Next(Tuple* out, RID* rid_out) {
    Tuple child_tuple;
    RID   child_rid;
    int   rows_deleted = 0;

    while (child_executor_->Next(&child_tuple, &child_rid)) {
        // 1. Remove from index if one exists (indexed on column 0)
        if (table_info_->index) {
            Value key_val = child_tuple.GetValue(child_executor_->OutputSchema(), 0);
            table_info_->index->Remove(key_val.AsInt());
        }

        // 2. Tombstone the slot in the heap
        if (table_info_->heap->DeleteTuple(child_rid)) {
            ++rows_deleted;
        }
    }

    // Emit count tuple
    *out = Tuple({Value(int32_t{rows_deleted})}, output_schema_);
    return true;
}