#include "storage/tuple.h"
#include <cstring>

 Tuple::Tuple(const char* data, uint16_t len){
    data_.resize(len);
    memcpy(data_.data(),data,len);
 }
// Constructor handles Serialization right at creation
Tuple::Tuple(const std::vector<Value>& values, const Schema& schema) {
    for (size_t i = 0; i < schema.GetColumnCount(); ++i) {
        auto type = schema.GetColumns()[i].type;
        
        if (type == TypeId::INT) {
            int32_t val = values[i].AsInt();
            size_t old_size = data_.size();
            data_.resize(old_size + sizeof(int32_t));
            std::memcpy(&data_[old_size], &val, sizeof(int32_t));
        }
        else if (type == TypeId::BOOL) {
            bool val = values[i].AsBool();
            data_.push_back(val ? 1 : 0);
        }
        else if (type == TypeId::VARCHAR) {
            std::string s = values[i].AsString();
            uint16_t len = static_cast<uint16_t>(s.length());
            
            size_t old_size = data_.size();
            data_.resize(old_size + sizeof(uint16_t));
            std::memcpy(&data_[old_size], &len, sizeof(uint16_t));
            
            old_size = data_.size();
            data_.resize(old_size + len);
            std::memcpy(&data_[old_size], s.data(), len);
        }
    }
}

// Deserializes using the tuple's internal data_ array
std::vector<Value> Tuple::Deserialize(const Schema& schema) const {
    std::vector<Value> deserialisedValue;
    const char* raw_bytes = data_.data(); // Point to our own buffer
    
    for (size_t i = 0; i < schema.GetColumnCount(); i++) {
        auto type = schema.GetColumns()[i].type;
        
        if (type == TypeId::INT) {
            int32_t value;
            std::memcpy(&value, raw_bytes, sizeof(int32_t));
            raw_bytes += sizeof(int32_t);
            deserialisedValue.push_back(Value(value));
        }
        else if (type == TypeId::BOOL) {
            bool value;
            std::memcpy(&value, raw_bytes, sizeof(uint8_t));
            raw_bytes += sizeof(uint8_t);
            deserialisedValue.push_back(Value(value));
        }
        else if (type == TypeId::VARCHAR) {
            uint16_t len;
            std::memcpy(&len, raw_bytes, sizeof(uint16_t));
            raw_bytes += sizeof(uint16_t);
            
            std::string s(raw_bytes, len);
            raw_bytes += len;
            deserialisedValue.push_back(Value(s)); 
        }
    }
    return deserialisedValue;
}