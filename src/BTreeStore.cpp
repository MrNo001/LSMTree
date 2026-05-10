#include "../h/BTreeStore.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <vector>

namespace {

constexpr std::size_t kPageSize = 4096;
constexpr char kMagic[8] = {'B', 'T', 'R', 'E', 'E', '0', '2', '\0'};

struct Superblock {
    char magic[8];
    std::uint32_t root_page_id;
    std::uint32_t highest_page_id;
};

constexpr std::size_t kSuperblockBytes = sizeof(Superblock);

inline std::uint32_t readU32(const std::uint8_t* p) {
    std::uint32_t v = 0;
    std::memcpy(&v, p, 4);
    return v;
}

inline void writeU32(std::uint8_t* p, std::uint32_t v) {
    std::memcpy(p, &v, 4);
}

inline std::uint16_t readU16(const std::uint8_t* p) {
    std::uint16_t v = 0;
    std::memcpy(&v, p, 2);
    return v;
}

inline void writeU16(std::uint8_t* p, std::uint16_t v) {
    std::memcpy(p, &v, 2);
}

inline Slot* slotsBegin(std::uint8_t* page) {
    return reinterpret_cast<Slot*>(page + sizeof(PageHeader));
}

inline const Slot* slotsBegin(const std::uint8_t* page) {
    return reinterpret_cast<const Slot*>(page + sizeof(PageHeader));
}

inline std::size_t slotDirBytes(std::uint16_t slot_count) {
    return sizeof(PageHeader) + static_cast<std::size_t>(slot_count) * sizeof(Slot);
}

inline std::size_t leafRecordSize(const std::string& payload) {
    return 4 + 2 + payload.size();
}

inline std::size_t internalRecordSize() {
    return 8;
}

double occupancyPayloadRatio(const std::uint8_t* page) {
    const PageHeader* h = reinterpret_cast<const PageHeader*>(page);
    const std::size_t slot_part = static_cast<std::size_t>(h->slot_count) * sizeof(Slot);
    const std::size_t heap_used = kPageSize - static_cast<std::size_t>(h->free_ptr);
    const std::size_t payload_used = slot_part + heap_used;
    const double cap = static_cast<double>(kPageSize - sizeof(PageHeader));
    if (cap <= 0.0) {
        return 1.0;
    }
    return static_cast<double>(payload_used) / cap;
}

std::uint32_t leafSlotKey(const std::uint8_t* page, int i) {
    const Slot& s = slotsBegin(page)[i];
    return readU32(page + s.offset);
}

std::uint32_t internalSlotKey(const std::uint8_t* page, int i) {
    const Slot& s = slotsBegin(page)[i];
    return readU32(page + s.offset);
}

std::uint32_t internalSlotRightChild(const std::uint8_t* page, int i) {
    const Slot* slot_array = slotsBegin(page);
    const Slot& s = slot_array[i];
    return readU32(page + s.offset + 4);
}

void decodeLeafRecord(const std::uint8_t* page, const Slot& s, std::uint32_t* key, std::string* payload) {
    const std::uint8_t* r = page + s.offset;
    *key = readU32(r);
    const std::uint16_t len = readU16(r + 4);
    payload->assign(reinterpret_cast<const char*>(r + 6), len);
}

std::size_t encodeLeafRecord(std::uint8_t* dst, std::uint32_t key, const std::string& payload) {
    writeU32(dst, key);
    writeU16(dst + 4, static_cast<std::uint16_t>(payload.size()));
    if (!payload.empty()) {
        std::memcpy(dst + 6, payload.data(), payload.size());
    }
    return 6 + payload.size();
}

std::size_t encodeInternalRecord(std::uint8_t* dst, std::uint32_t key, std::uint32_t right_child) {
    writeU32(dst, key);
    writeU32(dst + 4, right_child);
    return 8;
}

/** Pack records contiguously at bottom of page; maximize free_ptr. */
void defragPage(std::uint8_t* page) {
    PageHeader* h = reinterpret_cast<PageHeader*>(page);
    if (h->slot_count == 0) {
        h->free_ptr = static_cast<std::uint16_t>(kPageSize);
        return;
    }

    struct Item {
        std::vector<std::uint8_t> bytes;
    };
    std::vector<Item> items;
    items.reserve(h->slot_count);
    for (int i = 0; i < h->slot_count; ++i) {
        const Slot& s = slotsBegin(page)[i];
        Item it;
        it.bytes.assign(page + s.offset, page + s.offset + s.size);
        items.push_back(std::move(it));
    }

    std::size_t cursor = kPageSize;
    for (int i = 0; i < h->slot_count; ++i) {
        cursor -= items[static_cast<std::size_t>(i)].bytes.size();
        std::memcpy(page + cursor, items[static_cast<std::size_t>(i)].bytes.data(),
                    items[static_cast<std::size_t>(i)].bytes.size());
        slotsBegin(page)[i].offset = static_cast<std::uint16_t>(cursor);
        slotsBegin(page)[i].size = static_cast<std::uint16_t>(items[static_cast<std::size_t>(i)].bytes.size());
    }
    h->free_ptr = static_cast<std::uint16_t>(cursor);
}

bool physicalCanFit(const std::uint8_t* page, std::size_t extra_slots_bytes, std::size_t new_record_bytes) {
    const PageHeader* h = reinterpret_cast<const PageHeader*>(page);
    const std::size_t slot_end = slotDirBytes(h->slot_count) + extra_slots_bytes;
    const std::size_t heap_need = (kPageSize - static_cast<std::size_t>(h->free_ptr)) + new_record_bytes;
    const std::size_t heap_avail = kPageSize - slot_end;
    return heap_need <= heap_avail;
}

void initBlankPage(std::uint8_t* page, std::uint32_t page_id, bool is_leaf) {
    std::memset(page, 0, kPageSize);
    PageHeader* h = reinterpret_cast<PageHeader*>(page);
    h->page_id = page_id;
    h->parent_id = 0;
    h->next_page_id = 0;
    h->prev_page_id = 0;
    h->slot_count = 0;
    h->free_ptr = static_cast<std::uint16_t>(kPageSize);
    h->is_leaf = is_leaf ? 1 : 0;
}

int leafLowerBound(const std::uint8_t* page, std::uint32_t key) {
    const PageHeader* h = reinterpret_cast<const PageHeader*>(page);
    int lo = 0;
    int hi = h->slot_count;
    while (lo < hi) {
        const int mid = (lo + hi) / 2;
        const std::uint32_t mk = leafSlotKey(page, mid);
        if (mk < key) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    return lo;
}

int internalUpperBound(const std::uint8_t* page, std::uint32_t key) {
    const PageHeader* h = reinterpret_cast<const PageHeader*>(page);
    int lo = 0;
    int hi = h->slot_count;
    while (lo < hi) {
        const int mid = (lo + hi) / 2;
        const std::uint32_t mk = internalSlotKey(page, mid);
        if (mk <= key) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    return lo;
}

std::uint32_t internalChildAt(const std::uint8_t* page, int child_index) {
    const PageHeader* h = reinterpret_cast<const PageHeader*>(page);
    if (child_index == 0) {
        return h->prev_page_id;
    }
    return internalSlotRightChild(page, child_index - 1);
}

/** Insert/replace leaf entry; page buffer must be writable. Caller handles split/defrag policy. */
bool leafPut(std::uint8_t* page, std::uint32_t key, const std::string& payload) {
    PageHeader* h = reinterpret_cast<PageHeader*>(page);
    const int pos = leafLowerBound(page, key);
    const std::size_t rec_sz = leafRecordSize(payload);

    if (pos < h->slot_count && leafSlotKey(page, pos) == key) {
        const Slot old = slotsBegin(page)[pos];
        if (old.size == rec_sz) {
            encodeLeafRecord(page + old.offset, key, payload);
            return true;
        }
        /* Remove old slot payload by shifting slots and patching heap — use rebuild without old */
        std::vector<std::pair<std::uint32_t, std::string>> tmp;
        tmp.reserve(static_cast<std::size_t>(h->slot_count));
        for (int i = 0; i < h->slot_count; ++i) {
            std::uint32_t k = 0;
            std::string pl;
            decodeLeafRecord(page, slotsBegin(page)[i], &k, &pl);
            if (i != pos) {
                tmp.emplace_back(k, std::move(pl));
            }
        }
        tmp.emplace_back(key, payload);
        std::sort(tmp.begin(), tmp.end(),
                  [](const auto& a, const auto& b) { return a.first < b.first; });
        h->slot_count = 0;
        h->free_ptr = static_cast<std::uint16_t>(kPageSize);
        for (const auto& kv : tmp) {
            const std::size_t rs = leafRecordSize(kv.second);
            if (!physicalCanFit(page, sizeof(Slot), rs)) {
                return false;
            }
            h->slot_count += 1;
            std::size_t cursor = h->free_ptr;
            cursor -= rs;
            encodeLeafRecord(page + cursor, kv.first, kv.second);
            slotsBegin(page)[h->slot_count - 1].offset = static_cast<std::uint16_t>(cursor);
            slotsBegin(page)[h->slot_count - 1].size = static_cast<std::uint16_t>(rs);
            h->free_ptr = static_cast<std::uint16_t>(cursor);
        }
        defragPage(page);
        return true;
    }

    if (!physicalCanFit(page, sizeof(Slot), rec_sz)) {
        return false;
    }

    /* Shift slots right from pos */
    h->slot_count += 1;
    for (int i = h->slot_count - 1; i > pos; --i) {
        slotsBegin(page)[i] = slotsBegin(page)[i - 1];
    }

    std::size_t cursor = h->free_ptr;
    cursor -= rec_sz;
    encodeLeafRecord(page + cursor, key, payload);
    slotsBegin(page)[pos].offset = static_cast<std::uint16_t>(cursor);
    slotsBegin(page)[pos].size = static_cast<std::uint16_t>(rec_sz);
    h->free_ptr = static_cast<std::uint16_t>(cursor);
    return true;
}

void unpackInternal(const std::uint8_t* page, std::vector<std::uint32_t>* keys,
                    std::vector<std::uint32_t>* ptrs) {
    const PageHeader* h = reinterpret_cast<const PageHeader*>(page);
    keys->clear();
    ptrs->clear();
    ptrs->push_back(h->prev_page_id);
    for (int i = 0; i < h->slot_count; ++i) {
        keys->push_back(internalSlotKey(page, i));
        ptrs->push_back(internalSlotRightChild(page, i));
    }
}

void repackInternal(std::uint8_t* page, const std::vector<std::uint32_t>& keys,
                    const std::vector<std::uint32_t>& ptrs) {
    PageHeader* h = reinterpret_cast<PageHeader*>(page);
    h->is_leaf = 0;
    h->next_page_id = 0;
    h->slot_count = 0;
    h->free_ptr = static_cast<std::uint16_t>(kPageSize);
    h->prev_page_id = ptrs.front();
    for (std::size_t i = 0; i < keys.size(); ++i) {
        const std::size_t rec_sz = internalRecordSize();
        std::size_t cursor = h->free_ptr;
        cursor -= rec_sz;
        encodeInternalRecord(page + cursor, keys[i], ptrs[i + 1]);
        h->slot_count += 1;
        slotsBegin(page)[static_cast<int>(i)].offset = static_cast<std::uint16_t>(cursor);
        slotsBegin(page)[static_cast<int>(i)].size = static_cast<std::uint16_t>(rec_sz);
        h->free_ptr = static_cast<std::uint16_t>(cursor);
    }
    defragPage(page);
}

void collectLeafEntries(const std::uint8_t* page, std::vector<std::pair<std::uint32_t, std::string>>* out) {
    const PageHeader* h = reinterpret_cast<const PageHeader*>(page);
    out->clear();
    for (int i = 0; i < h->slot_count; ++i) {
        std::uint32_t k = 0;
        std::string pl;
        decodeLeafRecord(page, slotsBegin(page)[i], &k, &pl);
        out->emplace_back(k, std::move(pl));
    }
}

void upsertEntry(std::vector<std::pair<std::uint32_t, std::string>>* items, std::uint32_t key,
                 const std::string& pl) {
    const auto it =
        std::lower_bound(items->begin(), items->end(), key,
                         [](const std::pair<std::uint32_t, std::string>& a, std::uint32_t k) {
                             return a.first < k;
                         });
    if (it != items->end() && it->first == key) {
        it->second = pl;
    } else {
        items->insert(it, std::make_pair(key, pl));
    }
}

bool packLeafSorted(std::uint8_t* page, std::uint32_t page_id,
                    const std::vector<std::pair<std::uint32_t, std::string>>& items) {
    initBlankPage(page, page_id, true);
    for (const auto& kv : items) {
        if (!leafPut(page, kv.first, kv.second)) {
            return false;
        }
    }
    defragPage(page);
    return true;
}

/** Empty page with is_leaf=0 and no structural links is treated as a leaf (read/write repair). */
void normalizePageHeaderForRead(std::uint8_t* page) {
    PageHeader* h = reinterpret_cast<PageHeader*>(page);
    if (h->is_leaf == 0 && h->slot_count == 0 && h->prev_page_id == 0 && h->next_page_id == 0) {
        h->is_leaf = 1;
    }
}

}  // namespace

PageHeader* BTreeStore::header(std::uint8_t* page) {
    return reinterpret_cast<PageHeader*>(page);
}

const PageHeader* BTreeStore::header(const std::uint8_t* page) {
    return reinterpret_cast<const PageHeader*>(page);
}

const std::string BTreeStore::kTombstonePayload(8, '\xff');

BTreeStore::BTreeStore(std::filesystem::path dataDirectory, double splitOccupancy, double mergeOccupancy)
    : split_occ_(splitOccupancy), merge_occ_(mergeOccupancy) {
    const std::filesystem::path dir = std::filesystem::absolute(std::move(dataDirectory));
    storePath_ = dir / "btree_pages.dat";
    openOrCreateFile();
}

BTreeStore::~BTreeStore() {
    if (file_.is_open()) {
        file_.close();
    }
}

void BTreeStore::openOrCreateFile() {
    std::filesystem::create_directories(storePath_.parent_path());

    const bool exists = std::filesystem::exists(storePath_);
    if (!exists) {
        std::ofstream create(storePath_, std::ios::binary | std::ios::out | std::ios::trunc);
        if (!create.is_open()) {
            throw std::runtime_error("BTreeStore: failed to create " + storePath_.string());
        }
        create.close();
    }

    file_.open(storePath_, std::ios::binary | std::ios::in | std::ios::out);
    if (!file_.is_open()) {
        throw std::runtime_error("BTreeStore: failed to open " + storePath_.string());
    }

    if (!exists || std::filesystem::file_size(storePath_) < static_cast<std::uintmax_t>(kSuperblockBytes)) {
        Superblock sb{};
        std::memcpy(sb.magic, kMagic, 8);
        sb.root_page_id = 1;
        sb.highest_page_id = 1;
        file_.seekp(0);
        file_.write(reinterpret_cast<const char*>(&sb), sizeof(sb));

        std::array<std::uint8_t, kPageSize> page{};
        initBlankPage(page.data(), 1, true);
        file_.seekp(static_cast<std::streamoff>(1) * static_cast<std::streamoff>(kPageSize));
        file_.write(reinterpret_cast<const char*>(page.data()), kPageSize);
        file_.flush();
    } else {
        file_.flush();
    }
}

void BTreeStore::readSuper(std::uint32_t& root_page_id, std::uint32_t& highest_page_id) const {
    Superblock sb{};
    this->file_.seekg(0);
    this->file_.read(reinterpret_cast<char*>(&sb), sizeof(sb));
    if (std::memcmp(sb.magic, kMagic, 8) != 0) {
        root_page_id = 1;
        highest_page_id = 1;
        return;
    }
    root_page_id = sb.root_page_id;
    highest_page_id = sb.highest_page_id;
}

void BTreeStore::writeSuper(std::uint32_t root_page_id, std::uint32_t highest_page_id) const {
    Superblock sb{};
    std::memcpy(sb.magic, kMagic, 8);
    sb.root_page_id = root_page_id;
    sb.highest_page_id = highest_page_id;
    this->file_.seekp(0);
    this->file_.write(reinterpret_cast<const char*>(&sb), sizeof(sb));
    this->file_.flush();
}

void BTreeStore::writePage(std::uint32_t page_id, const std::uint8_t* data) const {
    this->file_.seekp(static_cast<std::streamoff>(page_id) *
                                                 static_cast<std::streamoff>(kPageSize));
    this->file_.write(reinterpret_cast<const char*>(data), kPageSize);
    this->file_.flush();
}

void BTreeStore::readPage(std::uint32_t page_id, std::uint8_t* data) const {
    this->file_.seekg(static_cast<std::streamoff>(page_id) * static_cast<std::streamoff>(kPageSize));
    this->file_.read(reinterpret_cast<char*>(data), kPageSize);
}

std::uint32_t BTreeStore::allocatePageId(bool is_leaf) {
    std::uint32_t root_id = 0;
    std::uint32_t high = 0;
    readSuper(root_id, high);
    const std::uint32_t new_id = high + 1;
    std::array<std::uint8_t, kPageSize> page{};
    initBlankPage(page.data(), new_id, is_leaf);
    writePage(new_id, page.data());
    writeSuper(root_id, new_id);
    return new_id;
}

void BTreeStore::setChildrenParent(std::uint32_t parent_page_id, const std::uint8_t* page) {
    const PageHeader* h = header(page);
    if (h->is_leaf) {
        return;
    }
    for (int i = 0; i <= h->slot_count; ++i) {
        const std::uint32_t ch = internalChildAt(page, i);
        std::array<std::uint8_t, kPageSize> buf{};
        readPage(ch, buf.data());
        header(buf.data())->parent_id = parent_page_id;
        writePage(ch, buf.data());
    }
}

std::uint32_t BTreeStore::findLeafPage(std::uint32_t root_page_id, std::uint32_t key) const {
    if (root_page_id == 0) {
        return 0;
    }
    std::array<std::uint8_t, kPageSize> buf{};
    std::uint32_t pid = root_page_id;
    for (;;) {
        readPage(pid, buf.data());
        normalizePageHeaderForRead(buf.data());
        const PageHeader* h = header(buf.data());
        if (h->is_leaf) {
            return pid;
        }
        const int idx = internalUpperBound(buf.data(), key);
        const std::uint32_t next = internalChildAt(buf.data(), idx);
        if (next == 0 || next == pid) {
            return 0;
        }
        pid = next;
    }
}

std::uint32_t BTreeStore::leftmostLeafPage(std::uint32_t root_page_id) const {
    if (root_page_id == 0) {
        return 0;
    }
    std::array<std::uint8_t, kPageSize> buf{};
    std::uint32_t pid = root_page_id;
    for (;;) {
        readPage(pid, buf.data());
        normalizePageHeaderForRead(buf.data());
        const PageHeader* h = header(buf.data());
        if (h->is_leaf) {
            return pid;
        }
        const std::uint32_t down = h->prev_page_id;
        if (down == 0 || down == pid) {
            return 0;
        }
        pid = down;
    }
}

void BTreeStore::insert(std::uint32_t key, std::unique_ptr<IBTreeValue> value) {
    std::string payload = value ? value->toStoreString() : std::string();
    std::uint32_t root_id = 0;
    std::uint32_t high = 0;
    readSuper(root_id, high);
    SplitResult sp = insertRecursive(root_id, key, payload, root_id, high);
    readSuper(root_id, high);
    if (sp.has_split) {
        const std::uint32_t new_root = allocatePageId(false);
        std::array<std::uint8_t, kPageSize> new_page{};
        PageHeader* nh = reinterpret_cast<PageHeader*>(new_page.data());
        nh->page_id = new_root;
        nh->parent_id = 0;
        nh->next_page_id = 0;
        nh->prev_page_id = root_id;
        nh->slot_count = 1;
        nh->free_ptr = static_cast<std::uint16_t>(kPageSize);
        nh->is_leaf = 0;

        std::size_t cursor = nh->free_ptr;
        cursor -= internalRecordSize();
        encodeInternalRecord(new_page.data() + cursor, sp.separator_key, sp.right_page_id);
        slotsBegin(new_page.data())[0].offset = static_cast<std::uint16_t>(cursor);
        slotsBegin(new_page.data())[0].size = static_cast<std::uint16_t>(internalRecordSize());
        nh->free_ptr = static_cast<std::uint16_t>(cursor);

        writePage(new_root, new_page.data());

        std::array<std::uint8_t, kPageSize> left{};
        readPage(root_id, left.data());
        header(left.data())->parent_id = new_root;
        writePage(root_id, left.data());

        std::array<std::uint8_t, kPageSize> right{};
        readPage(sp.right_page_id, right.data());
        header(right.data())->parent_id = new_root;
        writePage(sp.right_page_id, right.data());

        std::array<std::uint8_t, kPageSize> nr{};
        readPage(new_root, nr.data());
        setChildrenParent(new_root, nr.data());

        readSuper(root_id, high);
        writeSuper(new_root, high);
    } else {
        writeSuper(root_id, high);
    }
}

namespace {

bool internalSplit(std::fstream& file, std::uint32_t page_id, std::uint32_t new_right_id,
                   std::uint32_t* sep_key) {
    std::array<std::uint8_t, kPageSize> P{};
    file.seekg(static_cast<std::streamoff>(page_id) * static_cast<std::streamoff>(kPageSize));
    file.read(reinterpret_cast<char*>(P.data()), kPageSize);

    const std::uint32_t saved_parent = reinterpret_cast<const PageHeader*>(P.data())->parent_id;

    std::vector<std::uint32_t> keys;
    std::vector<std::uint32_t> ptrs;
    unpackInternal(P.data(), &keys, &ptrs);

    const int n = static_cast<int>(keys.size());
    if (n == 0) {
        return false;
    }
    const int mid = n / 2;
    *sep_key = keys[static_cast<std::size_t>(mid)];

    std::vector<std::uint32_t> lkeys(keys.begin(), keys.begin() + mid);
    std::vector<std::uint32_t> lptrs(ptrs.begin(), ptrs.begin() + static_cast<std::ptrdiff_t>(mid + 1));

    std::vector<std::uint32_t> rkeys(keys.begin() + static_cast<std::ptrdiff_t>(mid + 1), keys.end());
    std::vector<std::uint32_t> rptrs(ptrs.begin() + static_cast<std::ptrdiff_t>(mid + 1), ptrs.end());

    std::array<std::uint8_t, kPageSize> L{};
    std::array<std::uint8_t, kPageSize> R{};
    initBlankPage(L.data(), page_id, false);
    initBlankPage(R.data(), new_right_id, false);
    repackInternal(L.data(), lkeys, lptrs);
    repackInternal(R.data(), rkeys, rptrs);

    reinterpret_cast<PageHeader*>(L.data())->parent_id = saved_parent;
    reinterpret_cast<PageHeader*>(R.data())->parent_id = saved_parent;

    /* Fix parent ids for children */
    {
        PageHeader* hl = reinterpret_cast<PageHeader*>(L.data());
        for (int i = 0; i <= hl->slot_count; ++i) {
            const std::uint32_t ch = internalChildAt(L.data(), i);
            std::array<std::uint8_t, kPageSize> chb{};
            file.seekg(static_cast<std::streamoff>(ch) * static_cast<std::streamoff>(kPageSize));
            file.read(reinterpret_cast<char*>(chb.data()), kPageSize);
            reinterpret_cast<PageHeader*>(chb.data())->parent_id = page_id;
            file.seekp(static_cast<std::streamoff>(ch) * static_cast<std::streamoff>(kPageSize));
            file.write(reinterpret_cast<const char*>(chb.data()), kPageSize);
        }
    }
    {
        PageHeader* hr = reinterpret_cast<PageHeader*>(R.data());
        for (int i = 0; i <= hr->slot_count; ++i) {
            const std::uint32_t ch = internalChildAt(R.data(), i);
            std::array<std::uint8_t, kPageSize> chb{};
            file.seekg(static_cast<std::streamoff>(ch) * static_cast<std::streamoff>(kPageSize));
            file.read(reinterpret_cast<char*>(chb.data()), kPageSize);
            reinterpret_cast<PageHeader*>(chb.data())->parent_id = new_right_id;
            file.seekp(static_cast<std::streamoff>(ch) * static_cast<std::streamoff>(kPageSize));
            file.write(reinterpret_cast<const char*>(chb.data()), kPageSize);
        }
    }

    file.seekp(static_cast<std::streamoff>(page_id) * static_cast<std::streamoff>(kPageSize));
    file.write(reinterpret_cast<const char*>(L.data()), kPageSize);
    file.seekp(static_cast<std::streamoff>(new_right_id) * static_cast<std::streamoff>(kPageSize));
    file.write(reinterpret_cast<const char*>(R.data()), kPageSize);
    return true;
}

}  // namespace

BTreeStore::SplitResult BTreeStore::insertRecursive(std::uint32_t page_id, std::uint32_t key,
                                                    const std::string& payload,
                                                    std::uint32_t& root_page_id,
                                                    std::uint32_t& highest_page_id) {
    (void)root_page_id;
    (void)highest_page_id;

    if (page_id == 0) {
        SplitResult bad;
        return bad;
    }

    std::array<std::uint8_t, kPageSize> buf{};
    readPage(page_id, buf.data());
    normalizePageHeaderForRead(buf.data());
    PageHeader* ph = header(buf.data());

    if (ph->is_leaf == 0 && ph->slot_count == 0 && ph->prev_page_id == 0 && ph->next_page_id == 0) {
        ph->is_leaf = 1;
        writePage(page_id, buf.data());
    }

    if (ph->is_leaf) {
        std::vector<std::pair<std::uint32_t, std::string>> items;
        collectLeafEntries(buf.data(), &items);
        upsertEntry(&items, key, payload);

        std::array<std::uint8_t, kPageSize> trial{};
        if (!packLeafSorted(trial.data(), page_id, items)) {
            if (items.size() < 2) {
                SplitResult bad;
                return bad;
            }
            const std::size_t mid = items.size() / 2;
            std::vector<std::pair<std::uint32_t, std::string>> left(
                items.begin(), items.begin() + static_cast<std::ptrdiff_t>(mid));
            std::vector<std::pair<std::uint32_t, std::string>> right(
                items.begin() + static_cast<std::ptrdiff_t>(mid), items.end());

            const std::uint32_t right_id = allocatePageId(true);
            const std::uint32_t old_next = ph->next_page_id;
            const std::uint32_t par = ph->parent_id;

            std::array<std::uint8_t, kPageSize> L{};
            std::array<std::uint8_t, kPageSize> R{};
            if (!packLeafSorted(L.data(), page_id, left) || !packLeafSorted(R.data(), right_id, right)) {
                SplitResult bad;
                return bad;
            }

            PageHeader* hl = header(L.data());
            PageHeader* hr = header(R.data());
            hl->parent_id = par;
            hr->parent_id = par;
            hl->next_page_id = right_id;
            hr->prev_page_id = page_id;
            hr->next_page_id = old_next;

            if (old_next != 0) {
                std::array<std::uint8_t, kPageSize> nx{};
                readPage(old_next, nx.data());
                header(nx.data())->prev_page_id = right_id;
                writePage(old_next, nx.data());
            }

            writePage(page_id, L.data());
            writePage(right_id, R.data());

            SplitResult out;
            out.has_split = true;
            out.separator_key = right.front().first;
            out.right_page_id = right_id;
            return out;
        }

        if (occupancyPayloadRatio(trial.data()) > split_occ_ && items.size() >= 2) {
            const std::size_t mid = items.size() / 2;
            std::vector<std::pair<std::uint32_t, std::string>> left(
                items.begin(), items.begin() + static_cast<std::ptrdiff_t>(mid));
            std::vector<std::pair<std::uint32_t, std::string>> right(
                items.begin() + static_cast<std::ptrdiff_t>(mid), items.end());

            const std::uint32_t right_id = allocatePageId(true);
            const std::uint32_t old_next = ph->next_page_id;
            const std::uint32_t par = ph->parent_id;

            std::array<std::uint8_t, kPageSize> L{};
            std::array<std::uint8_t, kPageSize> R{};
            if (!packLeafSorted(L.data(), page_id, left) || !packLeafSorted(R.data(), right_id, right)) {
                SplitResult bad;
                return bad;
            }

            PageHeader* hl = header(L.data());
            PageHeader* hr = header(R.data());
            hl->parent_id = par;
            hr->parent_id = par;
            hl->next_page_id = right_id;
            hr->prev_page_id = page_id;
            hr->next_page_id = old_next;

            if (old_next != 0) {
                std::array<std::uint8_t, kPageSize> nx{};
                readPage(old_next, nx.data());
                header(nx.data())->prev_page_id = right_id;
                writePage(old_next, nx.data());
            }

            writePage(page_id, L.data());
            writePage(right_id, R.data());

            SplitResult out;
            out.has_split = true;
            out.separator_key = right.front().first;
            out.right_page_id = right_id;
            return out;
        }

        writePage(page_id, trial.data());
        SplitResult none;
        return none;
    }

    /* Internal node */
    const int ch_idx = internalUpperBound(buf.data(), key);
    const std::uint32_t child = internalChildAt(buf.data(), ch_idx);
    if (child == 0 || child == page_id) {
        SplitResult bad;
        return bad;
    }
    SplitResult ch = insertRecursive(child, key, payload, root_page_id, highest_page_id);
    if (!ch.has_split) {
        return {};
    }

    readPage(page_id, buf.data());
    const std::uint32_t saved_parent = header(buf.data())->parent_id;
    std::vector<std::uint32_t> keys;
    std::vector<std::uint32_t> ptrs;
    unpackInternal(buf.data(), &keys, &ptrs);
    keys.insert(keys.begin() + ch_idx, ch.separator_key);
    ptrs.insert(ptrs.begin() + ch_idx + 1, ch.right_page_id);

    repackInternal(buf.data(), keys, ptrs);
    header(buf.data())->page_id = page_id;
    header(buf.data())->parent_id = saved_parent;
    header(buf.data())->is_leaf = 0;
    setChildrenParent(page_id, buf.data());
    writePage(page_id, buf.data());

    readPage(page_id, buf.data());
    defragPage(buf.data());
    writePage(page_id, buf.data());

    if (occupancyPayloadRatio(buf.data()) <= split_occ_) {
        SplitResult none;
        return none;
    }

    const std::uint32_t new_right = allocatePageId(false);
    std::uint32_t sep = 0;
    if (!internalSplit(file_, page_id, new_right, &sep)) {
        SplitResult bad;
        return bad;
    }

    std::array<std::uint8_t, kPageSize> left_pg{};
    std::array<std::uint8_t, kPageSize> right_pg{};
    readPage(page_id, left_pg.data());
    readPage(new_right, right_pg.data());
    setChildrenParent(page_id, left_pg.data());
    setChildrenParent(new_right, right_pg.data());
    writePage(page_id, left_pg.data());
    writePage(new_right, right_pg.data());

    SplitResult out;
    out.has_split = true;
    out.separator_key = sep;
    out.right_page_id = new_right;
    return out;
}

std::optional<std::string> BTreeStore::search(std::uint32_t key) const {
    std::uint32_t root_id = 0;
    std::uint32_t high = 0;
    readSuper(root_id, high);
    const std::uint32_t leaf = findLeafPage(root_id, key);
    std::array<std::uint8_t, kPageSize> buf{};
    readPage(leaf, buf.data());
    normalizePageHeaderForRead(buf.data());
    const int pos = leafLowerBound(buf.data(), key);
    const PageHeader* h = header(buf.data());
    if (pos >= h->slot_count || leafSlotKey(buf.data(), pos) != key) {
        return std::nullopt;
    }
    std::uint32_t k = 0;
    std::string pl;
    decodeLeafRecord(buf.data(), slotsBegin(buf.data())[pos], &k, &pl);
    if (pl == kTombstonePayload) {
        return std::nullopt;
    }
    return pl;
}

void BTreeStore::remove(std::uint32_t key) {
    class Tombstone final : public IBTreeValue {
    public:
        std::string toStoreString() const override {
            return BTreeStore::kTombstonePayload;
        }
    };
    insert(key, std::make_unique<Tombstone>());
    std::uint32_t root_id = 0;
    std::uint32_t high = 0;
    readSuper(root_id, high);
    std::uint32_t root_mut = root_id;
    maybeMergeAfterRemove(findLeafPage(root_id, key), root_mut, high);
    if (root_mut != root_id) {
        writeSuper(root_mut, high);
    }
}

bool BTreeStore::maybeMergeAfterRemove(std::uint32_t leaf_page_id, std::uint32_t& root_page_id,
                                       std::uint32_t highest_page_id) {
    (void)highest_page_id;
    std::array<std::uint8_t, kPageSize> L{};
    readPage(leaf_page_id, L.data());
    if (occupancyPayloadRatio(L.data()) >= merge_occ_) {
        return false;
    }

    const PageHeader* hl = header(L.data());
    const std::uint32_t right_id = hl->next_page_id;
    if (right_id == 0) {
        return false;
    }

    std::array<std::uint8_t, kPageSize> R{};
    readPage(right_id, R.data());

    /* Combined payload size estimate */
    std::size_t combined_slots =
        static_cast<std::size_t>(hl->slot_count + header(R.data())->slot_count) * sizeof(Slot);
    std::size_t combined_heap = (kPageSize - hl->free_ptr) + (kPageSize - header(R.data())->free_ptr);
    const std::size_t projected = combined_slots + combined_heap;
    const double cap = static_cast<double>(kPageSize - sizeof(PageHeader));
    if (static_cast<double>(projected) > cap * 0.98) {
        return false;
    }

    /* Merge R into L */
    const std::uint32_t new_next = header(R.data())->next_page_id;
    const std::uint32_t leaf_par = header(L.data())->parent_id;
    std::vector<std::pair<std::uint32_t, std::string>> merged;
    collectLeafEntries(L.data(), &merged);
    for (int i = 0; i < header(R.data())->slot_count; ++i) {
        std::uint32_t k = 0;
        std::string pl;
        decodeLeafRecord(R.data(), slotsBegin(R.data())[i], &k, &pl);
        merged.emplace_back(k, std::move(pl));
    }
    std::sort(merged.begin(), merged.end(),
              [](const std::pair<std::uint32_t, std::string>& a,
                 const std::pair<std::uint32_t, std::string>& b) { return a.first < b.first; });
    if (!packLeafSorted(L.data(), leaf_page_id, merged)) {
        return false;
    }
    header(L.data())->parent_id = leaf_par;
    header(L.data())->next_page_id = new_next;
    if (new_next != 0) {
        std::array<std::uint8_t, kPageSize> nn{};
        readPage(new_next, nn.data());
        header(nn.data())->prev_page_id = leaf_page_id;
        writePage(new_next, nn.data());
    }

    writePage(leaf_page_id, L.data());

    /* Remove separator key pointing to right_id from parent */
    const std::uint32_t parent_id = hl->parent_id;
    if (parent_id == 0) {
        return true;
    }

    std::array<std::uint8_t, kPageSize> P{};
    readPage(parent_id, P.data());
    std::vector<std::uint32_t> keys;
    std::vector<std::uint32_t> ptrs;
    unpackInternal(P.data(), &keys, &ptrs);

    int rm = -1;
    for (std::size_t i = 0; i < ptrs.size(); ++i) {
        if (ptrs[i] == right_id) {
            rm = static_cast<int>(i);
            break;
        }
    }
    if (rm < 0) {
        return true;
    }

    if (rm > 0) {
        keys.erase(keys.begin() + (rm - 1));
    }
    ptrs.erase(ptrs.begin() + rm);

    initBlankPage(P.data(), parent_id, false);
    repackInternal(P.data(), keys, ptrs);

    for (int i = 0; i <= reinterpret_cast<PageHeader*>(P.data())->slot_count; ++i) {
        const std::uint32_t ch = internalChildAt(P.data(), i);
        std::array<std::uint8_t, kPageSize> chb{};
        readPage(ch, chb.data());
        header(chb.data())->parent_id = parent_id;
        writePage(ch, chb.data());
    }

    writePage(parent_id, P.data());
    defragPage(P.data());
    writePage(parent_id, P.data());

    if (keys.empty() && ptrs.size() == 1 && parent_id == root_page_id) {
        const std::uint32_t only = ptrs.front();
        std::array<std::uint8_t, kPageSize> child{};
        readPage(only, child.data());
        header(child.data())->parent_id = 0;
        writePage(only, child.data());
        writeSuper(only, highest_page_id);
        root_page_id = only;
    }

    return true;
}

std::vector<std::pair<std::uint32_t, std::string>> BTreeStore::rangeQuery(std::uint32_t startKey,
                                                                         std::uint32_t endKey) const {
    std::uint32_t a = startKey;
    std::uint32_t b = endKey;
    if (a > b) {
        std::swap(a, b);
    }

    std::uint32_t root_id = 0;
    std::uint32_t high = 0;
    readSuper(root_id, high);

    std::vector<std::pair<std::uint32_t, std::string>> out;
    std::uint32_t leaf = findLeafPage(root_id, a);
    if (leaf == 0) {
        leaf = leftmostLeafPage(root_id);
    }

    std::size_t scan_guard = 0;
    while (leaf != 0) {
        ++scan_guard;
        if (scan_guard > static_cast<std::size_t>(high) + 8u) {
            break;
        }
        std::array<std::uint8_t, kPageSize> buf{};
        readPage(leaf, buf.data());
        normalizePageHeaderForRead(buf.data());
        const PageHeader* h = header(buf.data());
        for (int i = 0; i < h->slot_count; ++i) {
            std::uint32_t k = 0;
            std::string pl;
            decodeLeafRecord(buf.data(), slotsBegin(buf.data())[i], &k, &pl);
            if (k < a) {
                continue;
            }
            if (k > b) {
                return out;
            }
            if (pl != kTombstonePayload) {
                out.emplace_back(k, pl);
            }
        }
        const std::uint32_t nxt = h->next_page_id;
        if (nxt == leaf) {
            break;
        }
        leaf = nxt;
    }
    return out;
}
