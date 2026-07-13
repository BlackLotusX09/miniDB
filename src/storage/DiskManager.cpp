#include "storage/DiskManager.h"

// On open: if file exists, load saved next_page_id from meta; otherwise start at 0
DiskManager::DiskManager(string filename) : next_page_id(0) {
    meta_path_ = filename + ".meta";

    db_file.open(filename, ios::in | ios::out | ios::binary);
    if (!db_file.is_open()) {
        ofstream create(filename, ios::binary);
        create.close();
        db_file.open(filename, ios::in | ios::out | ios::binary);
    }

    LoadMeta();
}

DiskManager::~DiskManager() {
    SaveMeta();
    db_file.close();
}

void DiskManager::LoadMeta() {
    ifstream meta(meta_path_, ios::binary);
    if (meta.is_open()) {
        meta.read(reinterpret_cast<char*>(&next_page_id), sizeof(page_id_t));
    }
    // If no meta file exists, next_page_id stays 0 (fresh database)
}

void DiskManager::SaveMeta() {
    ofstream meta(meta_path_, ios::binary | ios::trunc);
    if (meta.is_open()) {
        meta.write(reinterpret_cast<const char*>(&next_page_id), sizeof(page_id_t));
    }
}

void DiskManager::WritePage(page_id_t page_id, const char* data) {
    db_file.clear();
    streamoff offset = static_cast<streamoff>(PAGE_SIZE) * page_id;
    db_file.seekp(offset);
    db_file.write(data, PAGE_SIZE);
    db_file.flush();
}

void DiskManager::ReadPage(page_id_t page_id, char* data) {
    db_file.clear();
    streamoff offset = static_cast<streamoff>(PAGE_SIZE) * page_id;
    db_file.seekg(offset);
    if (!db_file.read(data, PAGE_SIZE)) {
        streamsize bytes_read = db_file.gcount();
        if (bytes_read < PAGE_SIZE) {
            memset(data + bytes_read, 0, PAGE_SIZE - bytes_read);
        }
    }
}

page_id_t DiskManager::AllocatePages() {
    page_id_t pid = next_page_id++;
    SaveMeta();   // persist immediately so crashes don't lose track of allocated pages
    return pid;
}