#pragma once

#include <iostream>
#include<fstream>
#include<cstring>
#include"storage/page.h"

using namespace std;
class DiskManager{
    private:
    fstream db_file;
    page_id_t next_page_id;
    public:
    DiskManager(string filename);

    void WritePage(page_id_t page_id, const char* data);
    void ReadPage(page_id_t page_id, char* data);
    page_id_t AllocatePages();

};