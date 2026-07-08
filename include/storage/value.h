#pragma once
#include<string>
#include<vector>
#include "storage/schema.h"
using namespace std;
class Value{
private:
    TypeId type_;
    int32_t val_int_=0;
    bool val_bool=false;
    std::string val_str="";
public:
    Value(int32_t val) : type_(TypeId::INT), val_int_(val){}
    Value(bool val) : type_(TypeId::BOOL), val_bool(val){}
    Value(string val): type_(TypeId::VARCHAR), val_str(val){}

    TypeId  GetType() const{return type_;}
    int32_t AsInt() const{return val_int_;}
    bool AsBool() const {return val_bool;}
    std::string AsString () const{return val_str;}

};