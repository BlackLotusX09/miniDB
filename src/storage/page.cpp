#include "storage/page.h"

SlottedPage::SlottedPage(page_id_t page_id){
    memset(data_,0,PAGE_SIZE);

    Header()->page_id=page_id;
    Header()->num_slots=0;
    Header()->free_space_ptr=PAGE_SIZE;
}

PageHeader* SlottedPage::Header(){
    return reinterpret_cast<PageHeader*>(data_);
}
const PageHeader* SlottedPage::Header() const{
    return reinterpret_cast<const PageHeader*>(data_);
}
SlotEntry* SlottedPage::Slots(){
    return reinterpret_cast<SlotEntry*>(data_+sizeof(PageHeader));
}
const SlotEntry* SlottedPage::Slots()const {
    return reinterpret_cast<const SlotEntry*>(data_+sizeof(PageHeader));
}
char* SlottedPage::GetData(){
    return data_;
}
const char* SlottedPage::GetData()const{
    return data_;
}
uint16_t SlottedPage::GetFreeSpace()const{
    uint16_t slot_array_end=sizeof(PageHeader)+Header()->num_slots*sizeof(SlotEntry);
    return Header()->free_space_ptr-slot_array_end;
}
const char* SlottedPage::GetRecord(slot_id_t slot_id,uint16_t* len)const {
    if (slot_id >= Header()->num_slots)
        return nullptr;

    const SlotEntry* slots =
        Slots();

    *len = slots[slot_id].length;
    if(*len==0)return nullptr;
    return data_ +slots[slot_id].offset;
}
bool SlottedPage::InsertRecord(const char* record,uint16_t len,slot_id_t* out_slot)
{
    uint16_t needed =
        len + sizeof(SlotEntry);

    if (GetFreeSpace() < needed)
        return false;

    Header()->free_space_ptr -= len;

    uint16_t offset =
        Header()->free_space_ptr;

    memcpy( data_ + offset,record,len);

    SlotEntry* slots = Slots();

    slots[Header()->num_slots] ={offset,len};

    *out_slot = Header()->num_slots;

    Header()->num_slots++;

    return true;
}

bool SlottedPage::DeleteRecord(slot_id_t slot_id){
    if(slot_id>=Header()->num_slots){
        return false;
    }
    SlotEntry* slot= Slots();
    slot[slot_id].length=0;
    return true;
}