#pragma once
#include"page.h"
#include"BufferPool.h"
#include"b_tree_internal_page.h"
#include"b_tree_leaf_page.h"
#include"rid.h"
#include<cassert>
#include<cstring>
using namespace std;

struct btreeHeader{
    page_id_t root_page_id;
    int32_t order;
};

class BPlusTree{
public:
    explicit BPlusTree(BufferPoolManager* bpm, page_id_t headerPage): bpm_(bpm){}
    RID Search(int32_t key);
    bool isLeaf(Page* page);
private:
    BufferPoolManager* bpm_;
    page_id_t header_page_id;
};