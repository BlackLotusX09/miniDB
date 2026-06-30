#include "storage/page.h"

SlottedPage::SlottedPage(page_id_t page_id){
    memset(GetData(),0,PAGE_SIZE);

    Header()->page_id=page_id;
    Header()->num_slots=0;
    Header()->free_space_ptr=PAGE_SIZE;
}
void SlottedPage::Init(page_id_t page_id) {
    memset(GetData(), 0, PAGE_SIZE);
    Header()->page_id = page_id;
    Header()->num_slots = 0;
    Header()->free_space_ptr = PAGE_SIZE; // Set to 4096, not 0!
}
char* Page::GetData(){
    return data_;
}
const char* Page::GetData()const{
    return data_;
}
PageHeader* SlottedPage::Header(){
    PageHeader* header = reinterpret_cast<PageHeader*>(GetData());
    if (header->free_space_ptr == 0 && header->num_slots == 0) {
        header->free_space_ptr = PAGE_SIZE;
    }
    return header;
}
const PageHeader* SlottedPage::Header() const{
    const PageHeader* header = reinterpret_cast<const PageHeader*>(GetData());
    if (header->free_space_ptr == 0 && header->num_slots == 0) {
        const_cast<PageHeader*>(header)->free_space_ptr = PAGE_SIZE;
    }
    return header;
}
SlotEntry* SlottedPage::Slots(){
    return reinterpret_cast<SlotEntry*>(GetData()+sizeof(PageHeader));
}
const SlotEntry* SlottedPage::Slots()const {
    return reinterpret_cast<const SlotEntry*>(GetData()+sizeof(PageHeader));
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
    return GetData() +slots[slot_id].offset;
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

    memcpy( GetData() + offset,record,len);

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
