#include "storage/BufferPool.h"

BufferPoolManager::BufferPoolManager(DiskManager* dm)
 : disk_manager_(dm)
{
    for(int i=0;i<BUFFER_SIZE;i++){
        freePageList.push(i);
        frames_[i].is_dirty=false;
        frames_[i].pin_count=0;
        frames_[i].page_id=0;
        memset(frames_[i].page.GetData(),0,PAGE_SIZE);
        
        pte[i].frame_id=i;
        pte[i].pin_count=0;
        pte[i].is_dirty=false;
        pte[i].is_dirty=0;

    } 
}
Page* BufferPoolManager::FetchPage(page_id_t pid)
{
    if(page_table_.count(pid)){
        frame_id_t fid=page_table_[pid];
        if(frames_[fid].pin_count==0){
            lru_replacer.pin(fid);
        }
        frames_[fid].pin_count++;
        std::cout << "PID: " << pid << " pin_count shifted to: " << frames_[fid].pin_count << std::endl;
        return &frames_[fid].page;
    }
    if(freePageList.empty()){
    for (size_t i = 0; i < BUFFER_SIZE; ++i) {
    cout << "Frame " << i 
              << " -> Page ID: " << frames_[i].page_id
              << ", Pin Count: " << frames_[i].pin_count
              << ", Is Dirty: " << frames_[i].is_dirty <<endl;
                
    }
}
    frame_id_t fid=GetFreeFrame();
    if(fid==255)return nullptr;
    disk_manager_->ReadPage(pid,frames_[fid].page.GetData());
    page_table_[pid]=fid;
    frames_[fid].page_id=pid;
    frames_[fid].pin_count=1;
    frames_[fid].is_dirty=false;
    return &frames_[fid].page;
    

}
frame_id_t BufferPoolManager::GetFreeFrame() {
    if (!freePageList.empty()) {
        int fid = freePageList.front();
        freePageList.pop();
        return fid;
    }
    frame_id_t evicted_fid = lru_replacer.evict(frames_);
    if (evicted_fid != 255) {
        Frame& victim = frames_[evicted_fid];
        
        if (victim.is_dirty) {
            disk_manager_->WritePage(victim.page_id, victim.page.GetData());
        }
        page_table_.erase(victim.page_id);
        
        return evicted_fid;
    } 
    return 255;
}
void BufferPoolManager::UnpinPage(page_id_t pid, bool is_dirty)
{
    if(page_table_.count(pid) == 0)return;
    int frame_id = page_table_[pid];
    if (is_dirty) {
    frames_[frame_id].is_dirty = true;
    }
    
    assert(frames_[frame_id].pin_count > 0 && "Double-unpin bug detected! Pin count went negative.");
    std::cout << "PID: " << pid << " pin_count shifted to: " << frames_[frame_id].pin_count << std::endl;
    
    frames_[frame_id].pin_count--;
    if(frames_[frame_id].pin_count==0){
        lru_replacer.unpin(frame_id);
    }
    
}

void LRUReplacer::moveToFront(frameNode* node)
{
    if(node==head){
        return;
    }
    if(node->back != nullptr) {
        node->back->front = node->front;
    }
    if(node == tail){
        tail = node->back;
    } else {
        node->front->back = node->back;
    }
    node->back = nullptr;
    node->front = head;
    if(head == nullptr){
        head = node;
        tail = node;
    } else {
        head->back = node;
        head = node;
    }
}
frame_id_t LRUReplacer::evict(Frame* frames) {
    if (tail == nullptr) {
        return 255; 
    }

    // The tail is guaranteed to be the least recently used unpinned frame!
    frameNode* victim = tail;
    frame_id_t evicted_fid = victim->fid;

    // Remove from linked list
    tail = tail->back;
    if (tail != nullptr) {
        tail->front = nullptr;
    } else {
        head = nullptr; // List is now empty
    }

    // Clean up map tracking structures
    cache_map.erase(evicted_fid);
    delete victim; 

    return evicted_fid;
}
void LRUReplacer::unpin(frame_id_t fid){
    
    if(cache_map.find(fid)!=cache_map.end()){
        frameNode* node=cache_map[fid];
        moveToFront(node);
        return;
    }
    frameNode* newNode= new frameNode(fid);
    cache_map[fid]=newNode;
    newNode->front=head;
    if(head!=nullptr){
        head->back=newNode;
    }
    head=newNode;
    if(tail==nullptr){
        tail=newNode;
    }
}
void LRUReplacer::pin(frame_id_t fid){
    auto it = cache_map.find(fid);
    if(it==cache_map.end()){
        return;
    }
    frameNode* node= it->second;
    if(node->back!=nullptr){
        node->back->front=node->front;
    }
    if(node->front!=nullptr){
        node->front->back=node->back;
    }
    if(node==head){
        head=node->front;
    }
    if(node==tail){
        tail=node->back;
    }
    cache_map.erase(it);
    delete node;

}
void BufferPoolManager::flushAllPages(){
    for(int i=0;i<BUFFER_SIZE;i++){
        if(frames_[i].page_id !=0 && frames_[i].is_dirty){
            disk_manager_->WritePage(frames_[i].page_id,frames_[i].page.GetData());
            frames_[i].is_dirty = false;
        }
        
    }
}
