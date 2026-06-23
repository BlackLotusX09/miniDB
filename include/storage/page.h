#pragma once
#include<iostream>
#include<cstdint>
constexpr size_t PAGE_SIZE=4096;
#define INVALID_PAGE_ID -1
using page_id_t = int32_t;
class Page{
    public:
    static const size_t SIZE = 4096;
    page_id_t page_id_;
    char data_[SIZE];

    char* GetData(){return data_;}
    page_id_t GetPageId(){return page_id_;}
    
};