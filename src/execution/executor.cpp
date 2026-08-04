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