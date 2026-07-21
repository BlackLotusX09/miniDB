#pragma once
#include<iostream>
#include<cstdint>
#include "storage/page.h"
struct RID{
    page_id_t page_id;
    slot_id_t slot_id;
public:
    RID(page_id_t pid, slot_id_t slot_id);
    RID();
    
    bool operator==(const RID& other) const{
        return page_id == other.page_id && slot_id == other.slot_id;
    }
};
