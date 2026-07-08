#pragma once
#include<iostream>
#include<vector>
#include<string>
using namespace std;
// Sketching the logical structures
enum class TypeId { INT, BOOL, VARCHAR };

struct Column {
    string name;
    TypeId type;
    uint32_t length; // Only used for VARCHAR max capacity if needed
};

class Schema {
private:
    vector<Column> columns_;
public:
    Schema(vector<Column> columns) : columns_(move(columns)) {}
    const vector<Column>& GetColumns() const { return columns_; }
    size_t GetColumnCount() const { return columns_.size(); }
};