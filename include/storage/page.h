#pragma once
#include<iostream>
#include<cstdint>
static const uint32_t PAGE_SIZE = 4096;
#define INVALID_PAGE_ID -1

using slot_id_t= int32_t;
using page_id_t = uint32_t;
struct PageHeader{
    uint32_t page_id;
    uint16_t num_slots;
    uint16_t free_space_ptr;
};
struct SlotEntry{ 
    uint16_t offset; 
    uint16_t length;
};
class Page {
private:
    char data_[PAGE_SIZE];

public:
    char* GetData();
    const char* GetData() const;

    //void ResetMemory();
};
class SlottedPage:public Page{
public:
    explicit SlottedPage(page_id_t page_id=0);
    uint16_t GetFreeSpace() const;

    bool InsertRecord(const char* record,uint16_t len,slot_id_t* out_slot);

    const char* GetRecord(slot_id_t slot_id,uint16_t* len)const;

    PageHeader* Header();
    const PageHeader* Header() const;

    SlotEntry* Slots();
    const SlotEntry* Slots() const;

    bool DeleteRecord(slot_id_t slot_id);
    void Init(page_id_t page_id);

};
