#pragma once
#include<iostream>
#include<unordered_map>
#include<cassert>
#include <queue>
#include<cstring>

#include"page.h"
#include "DiskManager.h"

using namespace std;
static const uint8_t BUFFER_SIZE=32;

using frame_id_t=uint8_t;

struct frameNode{
    frameNode* front;
    frameNode* back;
    frame_id_t fid;

    frameNode(frame_id_t fid){
        this->fid=fid;
        front=nullptr;
        back=nullptr;
    }
    
};
struct Frame{
    Page page;
    page_id_t page_id;
    bool is_dirty;
    uint16_t pin_count;
};

struct PageTableEntry{
    frame_id_t frame_id;
    bool is_dirty;
    uint16_t pin_count;
    uint32_t last_access;
};

class LRUReplacer{
private:
    frameNode* head=nullptr;
    frameNode* tail=nullptr;
    unordered_map<frame_id_t,frameNode*> cache_map;
public:
    void pin(frame_id_t fid);
    void unpin(frame_id_t fid);
    void moveToFront(frameNode* node);
    frame_id_t evict(Frame* frames);

};

class BufferPoolManager{
private:
    Frame frames_[BUFFER_SIZE];
    unordered_map<page_id_t,frame_id_t> page_table_;
    PageTableEntry pte[BUFFER_SIZE];
    queue<int>freePageList;
    DiskManager* disk_manager_;
    LRUReplacer lru_replacer;
public:
    BufferPoolManager(DiskManager* dm);
    Page* FetchPage(page_id_t pid);
    frame_id_t GetFreeFrame();
    void UnpinPage(page_id_t pid, bool is_dirty);
    void flushAllPages();
    Page* NewPage(page_id_t* pid);
};

