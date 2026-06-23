#include "storage/DiskManager.h"
#include "storage/page.h"

#include <cassert>
#include <cstring>
#include <iostream>

int main() {

    {
        DiskManager dm("data/test.db");

        char page0[PAGE_SIZE];
        char page1[PAGE_SIZE];
        char page2[PAGE_SIZE];

        memset(page0, 'A', PAGE_SIZE);
        memset(page1, 'B', PAGE_SIZE);
        memset(page2, 'C', PAGE_SIZE);

        dm.WritePage(0, page0);
        dm.WritePage(1, page1);
        dm.WritePage(2, page2);

        std::cout << "Pages written\n";
    }

    {
        DiskManager dm("data/test.db");

        char read0[PAGE_SIZE];
        char read1[PAGE_SIZE];
        char read2[PAGE_SIZE];

        dm.ReadPage(0, read0);
        dm.ReadPage(1, read1);
        dm.ReadPage(2, read2);

        for(int i = 0; i < PAGE_SIZE; i++) {
            assert(read0[i] == 'A');
        }

        for(int i = 0; i < PAGE_SIZE; i++) {
            assert(read1[i] == 'B');
        }

        for(int i = 0; i < PAGE_SIZE; i++) {
            assert(read2[i] == 'C');
        }

        std::cout << "All pages verified\n";
    }

    return 0;
}