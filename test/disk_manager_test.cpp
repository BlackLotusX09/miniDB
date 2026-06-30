#include "storage/DiskManager.h"
#include "storage/page.h"
#include "storage/BufferPool.h"

#include <cassert>
#include <iostream>
#include <string>
#include <vector>

using namespace std;

void Test200PageStress(DiskManager* disk_manager)
{
    std::cout << "Starting 200-Page Stress Test..." << std::endl;
    BufferPoolManager bpm(disk_manager);
    for(page_id_t pid = 1; pid<= 200 ; ++pid){
        Page* raw_page=bpm.FetchPage(pid);
        SlottedPage* slotted_Page=reinterpret_cast<SlottedPage*> (raw_page);
        string record_str="Data for Page "+ to_string(pid);
        slot_id_t out_slot;
        bool success= slotted_Page->InsertRecord(record_str.c_str(), record_str.length(), &out_slot);
        assert(success && "Failed to insert record into slotted page");
        bpm.UnpinPage(pid,true);
    }
    cout<<" SUCCESS: 200-Page Stress Test Passed!"<<endl;
}

void TestPinEvictionSafety(DiskManager* disk_manager) {
    BufferPoolManager bpm(disk_manager);
    std::cout << "Starting Pin Eviction Safety Test..." << std::endl;

    // 1. Pin a target page and keep it pinned
    page_id_t pinned_pid = 5;
    Page* pinned_page = bpm.FetchPage(pinned_pid);
    
    // Write an identifiable marker to it
    SlottedPage* slotted_pinned = reinterpret_cast<SlottedPage*>(pinned_page);
    slot_id_t slot;
    slotted_pinned->InsertRecord("PINNED_NODE", 11, &slot);

    // 2. Flood the buffer pool with other pages to trigger heavy eviction cycles
// In your test file loop:
    for (page_id_t pid = 10; pid < 50; ++pid) {
        Page* p = bpm.FetchPage(pid);
        
        // Explicitly initialize the page layout so it doesn't carry disk manager garbage
        SlottedPage* sp = reinterpret_cast<SlottedPage*>(p);
        sp->Init(pid); 
        
        bpm.UnpinPage(pid, false);
    }

    // 3. Re-fetch our pinned page and verify it was NEVER kicked out
    Page* verify_page = bpm.FetchPage(pinned_pid);
    SlottedPage* slotted_verify = reinterpret_cast<SlottedPage*>(verify_page);
    
    uint16_t len = 0;
    const char* data = slotted_verify->GetRecord(0, &len);
    std::string record_str(data, len);

    // If it was wrongfully evicted, this assertion will fail or read garbage
    assert(record_str == "PINNED_NODE" && "Safety Failure: A pinned page was evicted!");

    // Clean up both pins
    bpm.UnpinPage(pinned_pid, false); // Unpin from step 3
    bpm.UnpinPage(pinned_pid, false); // Unpin from step 1

    std::cout << " SUCCESS: Pinned pages are completely safe from eviction." << std::endl;
}
int main() {
    // Create a temporary disk manager file
    DiskManager disk_manager("test_db.db");

    // Run your validation suites
    TestPinEvictionSafety(&disk_manager);
    Test200PageStress(&disk_manager);

    // Test Shutdown Flush explicitly
    {
        BufferPoolManager bpm(&disk_manager);
        Page* p = bpm.FetchPage(999);
        SlottedPage* sp = reinterpret_cast<SlottedPage*>(p);
        slot_id_t s;
        sp->InsertRecord("Shutdown Persistence Data", 27, &s);
        
        bpm.UnpinPage(999, true); // Kept in memory cache as dirty

        // Simulate shutdown flush sequence
        bpm.flushAllPages();
    } // bpm goes out of scope here

    // Re-verify that data was committed successfully to the disk binary
    BufferPoolManager fresh_bpm(&disk_manager);
    Page* recovery_page = fresh_bpm.FetchPage(999);
    SlottedPage* sp_recovery = reinterpret_cast<SlottedPage*>(recovery_page);
    uint16_t len = 0;
    assert(sp_recovery->GetRecord(0, &len) != nullptr && "FlushAllPages failed to commit data to disk!");

    std::cout << "All Week 3 tests cleared successfully!" << std::endl;
    return 0;
}