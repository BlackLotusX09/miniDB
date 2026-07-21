#include "storage/rid.h"

RID::RID(page_id_t pid, slot_id_t slot_id){
    this->page_id=pid;
    this->slot_id=slot_id;
}

RID::RID() : page_id(INVALID_PAGE_ID), slot_id(0) {}
