#include"storage/DiskManager.h"

DiskManager::DiskManager(string filename) : next_page_id(0){
    db_file.open(filename, ios::in | ios::out | ios::binary);
    if(!db_file.is_open()){
        ofstream create(filename, ios::binary);
        db_file.open(filename, ios::in | ios::out | ios::binary);
    }
}
void DiskManager::WritePage(page_id_t page_id, const char* data){
    streamoff offset=PAGE_SIZE*page_id;
    db_file.seekg(offset);
    db_file.write(data,PAGE_SIZE);
    db_file.flush();
}
void DiskManager::ReadPage(page_id_t page_id, char* data){
    streamoff offset=PAGE_SIZE*page_id;
    db_file.seekp(offset);
    if(!db_file.read(data,PAGE_SIZE)){
        streamsize bytes_read = db_file.gcount();

        if(bytes_read<PAGE_SIZE){
            memset(data+bytes_read,0,PAGE_SIZE-bytes_read);
        }
    }
}
page_id_t DiskManager::AllocatePages(){
    return next_page_id++;
}