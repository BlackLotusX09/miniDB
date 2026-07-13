#pragma once

#include <iostream>
#include <fstream>
#include <cstring>
#include <string>
#include "storage/page.h"

using namespace std;

class DiskManager {
private:
    fstream db_file;
    string meta_path_;   // stores the .meta file path
    page_id_t next_page_id;

    void LoadMeta();
    void SaveMeta();

public:
    DiskManager(string filename);
    ~DiskManager();

    void WritePage(page_id_t page_id, const char* data);
    void ReadPage(page_id_t page_id, char* data);
    page_id_t AllocatePages();

    // Returns how many pages have been allocated so far (used for persistence)
    page_id_t GetNextPageId() const { return next_page_id; }
};