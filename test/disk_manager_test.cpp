#include "storage/DiskManager.h"
#include "storage/page.h"

#include <cassert>
#include <iostream>
#include <string>
#include <vector>

using namespace std;

int main() {
    vector<string> expected;
    vector<string> actual;

    size_t num_pages = 0;

    // =========================
    // PHASE 1: INSERT + WRITE
    // =========================
    {
        DiskManager dm("data/test.db");

        vector<SlottedPage> pages;
        pages.emplace_back(0);

        vector<string> records;

        // Generate 500 variable-length records
        for(int i = 0; i < 500; i++) {
            records.push_back(
                "record_" +
                to_string(i) +
                "_" +
                string((i % 150) + 1, 'A' + (i % 26))
            );
        }

        for(const auto& record : records) {
            bool inserted = false;
            slot_id_t slot_id;

            for(auto& page : pages) {
                if(page.InsertRecord(
                        record.c_str(),
                        record.size() + 1,
                        &slot_id))
                {
                    inserted = true;
                    expected.push_back(record);
                    break;
                }
            }

            if(!inserted) {
                page_id_t new_page_id = pages.size();

                pages.emplace_back(new_page_id);

                bool ok = pages.back().InsertRecord(
                    record.c_str(),
                    record.size() + 1,
                    &slot_id
                );

                assert(ok);

                expected.push_back(record);
            }
        }

        cout << "Pages created: "
             << pages.size()
             << "\n";

        for(const auto& page : pages) {
            dm.WritePage(
                page.Header()->page_id,
                page.GetData()
            );
        }

        num_pages = pages.size();

        cout << "Pages written to disk.\n";
    }

    // =========================
    // PHASE 2: RESTART + READ
    // =========================
    {
        DiskManager dm("data/test.db");

        for(page_id_t pid = 0;
            pid < num_pages;
            pid++)
        {
            SlottedPage page(pid);

            dm.ReadPage(
                pid,
                page.GetData()
            );

            cout << "Read Page "
                 << pid
                 << " Slots="
                 << page.Header()->num_slots
                 << "\n";

            for(slot_id_t sid = 0;
                sid < page.Header()->num_slots;
                sid++)
            {
                uint16_t len;

                const char* rec =
                    page.GetRecord(sid, &len);

                if(rec == nullptr)
                    continue;

                actual.push_back(string(rec));
                cout
            << "Recovered Page "
            << pid
            << " Slot "
            << sid
            << " -> "
            << rec
            << endl;
            }
        }
    }

    // =========================
    // PHASE 3: VERIFY
    // =========================
    assert(expected.size() == actual.size());
    cout << expected.size() << endl;
    cout << actual.size() << endl;

    sort(expected.begin(), expected.end());
    sort(actual.begin(), actual.end());

    assert(expected == actual);
    cout << "\n=================================\n";
    cout << "Persistence Test PASSED\n";
    cout << "Recovered "
         << actual.size()
         << " records successfully.\n";
    cout << "=================================\n";



    return 0;
}