

#pragma once
#include <vector>
#include <cstdint>
#include "storage/schema.h"
#include "value.h" 

class Tuple {
private:
    vector<char> data_; 
public:
    Tuple() = default;
    Tuple(const char* data, uint16_t len);
    Tuple(const vector<Value>& values, const Schema& schema);
    const char* GetData() const { return data_.data(); }
    uint32_t GetLength() const { return static_cast<uint32_t>(data_.size()); }
    vector<Value> Deserialize(const Schema& schema) const;
};