#include "execution/ExecutoEngine.h"

ExecutorEngine::ExecutorEngine(Catalog* catalog,BufferPoolManager* bpm){
    this->catalog_=catalog;
    this->bpm_=bpm;
}

std::unique_ptr<Executor> BuildExecutor(Statement* stmt){}

void ExecutorEngine::Execute(Statement* stmt, ResultSet* result){
    auto* executor = BuildExecutor(stmt);
    executor->Init();
    Tuple tuple;
    RID rid;
    while(executor->next(&tuple,&rid)){
        result->tuples.push_back(tuple);
    }
    executor->Close();
    result->schema = executor->OutputSchema();

}