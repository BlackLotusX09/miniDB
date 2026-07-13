#include "storage/page.h"
#include <cstring>

// ─── Page ─────────────────────────────────────────────────────────────────────

char* Page::GetData() {
    return data_;
}

const char* Page::GetData() const {
    return data_;
}

// ─── SlottedPage ──────────────────────────────────────────────────────────────

SlottedPage::SlottedPage(page_id_t pid) {
    Init(pid);
}

void SlottedPage::Init(page_id_t pid) {
    memset(GetData(), 0, PAGE_SIZE);
    PageHeader* h = reinterpret_cast<PageHeader*>(GetData());
    h->page_id       = pid;
    h->num_slots     = 0;
    h->free_space_ptr = PAGE_SIZE;
    h->next_page_id  = INVALID_PAGE_ID;   // ← critical: 0 is a valid page!
}

PageHeader* SlottedPage::Header() {
    PageHeader* h = reinterpret_cast<PageHeader*>(GetData());
    // Defensive fixup: if free_space_ptr is 0 and no slots exist, the page
    // was never explicitly Init'd (e.g. raw zeroed frame). Fix it in place.
    if (h->free_space_ptr == 0 && h->num_slots == 0) {
        h->free_space_ptr = PAGE_SIZE;
        h->next_page_id   = INVALID_PAGE_ID;
    }
    return h;
}

const PageHeader* SlottedPage::Header() const {
    const PageHeader* h = reinterpret_cast<const PageHeader*>(GetData());
    if (h->free_space_ptr == 0 && h->num_slots == 0) {
        PageHeader* mh = const_cast<PageHeader*>(h);
        mh->free_space_ptr = PAGE_SIZE;
        mh->next_page_id   = INVALID_PAGE_ID;
    }
    return h;
}

SlotEntry* SlottedPage::Slots() {
    return reinterpret_cast<SlotEntry*>(GetData() + sizeof(PageHeader));
}

const SlotEntry* SlottedPage::Slots() const {
    return reinterpret_cast<const SlotEntry*>(GetData() + sizeof(PageHeader));
}

uint16_t SlottedPage::GetFreeSpace() const {
    uint16_t slot_array_end =
        static_cast<uint16_t>(sizeof(PageHeader)) +
        Header()->num_slots * static_cast<uint16_t>(sizeof(SlotEntry));
    return Header()->free_space_ptr - slot_array_end;
}

const char* SlottedPage::GetRecord(slot_id_t slot_id, uint16_t* len) const {
    if (slot_id >= static_cast<slot_id_t>(Header()->num_slots))
        return nullptr;

    const SlotEntry* slots = Slots();
    *len = slots[slot_id].length;
    if (*len == 0) return nullptr;   // tombstoned slot
    return GetData() + slots[slot_id].offset;
}

bool SlottedPage::InsertRecord(const char* record, uint16_t len, slot_id_t* out_slot) {
    uint16_t needed = len + static_cast<uint16_t>(sizeof(SlotEntry));
    if (GetFreeSpace() < needed) return false;

    Header()->free_space_ptr -= len;
    uint16_t offset = Header()->free_space_ptr;

    memcpy(GetData() + offset, record, len);

    SlotEntry* slots = Slots();
    slots[Header()->num_slots] = {offset, len};
    *out_slot = Header()->num_slots;
    Header()->num_slots++;

    return true;
}

bool SlottedPage::DeleteRecord(slot_id_t slot_id) {
    if (slot_id >= static_cast<slot_id_t>(Header()->num_slots))
        return false;
    Slots()[slot_id].length = 0;   // tombstone
    return true;
}
