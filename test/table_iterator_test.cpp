#include "TableHeap.h"

using namespace std;
vector<string> generateNames(size_t count ){
    vector<string> first_names = {"Alice", "Bob", "Charlie", "David", "Emma", "Frank", "Grace", "Henry"};
    vector<string> last_names = {"Smith", "Johnson", "Williams", "Brown", "Jones", "Miller", "Davis", "Wilson"};
    vector<string> names;
    names.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        string name = first_names[i % first_names.size()] + " " + last_names[(i / first_names.size()) % last_names.size()] + "_" + to_string(i);
        names.push_back(name);
    }
    return names;
}
void runIntegrationTest(){
    string fileName  = "test_db.db";
    remove(fileName.c_str());
    const size_t no_rows = 1200;
    DiskManager *dm= new DiskManager(fileName);
    BufferPoolManager* bpm = new BufferPoolManager(dm);
    page_id_t first_page_id = 0;
    TableHeap tableHeap(bpm,first_page_id);
    Schema student_schema({
       { "id", TypeId::INT},
       {"name", TypeId::VARCHAR},
       {"GPA",TypeId::INT}
    });
    vector<string> generatedNames = generateNames(no_rows);
    


}