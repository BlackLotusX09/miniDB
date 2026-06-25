#include"storage/page.h"
#include <iostream>


int main() {
    SlottedPage page(1);

slot_id_t ids[5];

page.InsertRecord("1",2,&ids[0]);
page.InsertRecord("2",2,&ids[1]);
page.InsertRecord("3",2,&ids[2]);
page.InsertRecord("4",2,&ids[3]);
page.InsertRecord("5",2,&ids[4]);

page.DeleteRecord(ids[1]);
for(slot_id_t i=0;i<page.Header()->num_slots;i++)
{
    uint16_t len;
    const char* rec = page.GetRecord(i,&len);

    if(rec == nullptr)
        continue;

    std::cout << rec << std::endl;
}
}