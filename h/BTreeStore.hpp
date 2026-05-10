#ifndef BTREESTORE_HPP
#define BTREESTORE_HPP

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#pragma pack(push, 1)
struct PageHeader {
    std::uint32_t page_id;
    std::uint32_t parent_id;
    std::uint32_t next_page_id;  // Leaf: right sibling. Internal: unused (0).
    std::uint32_t prev_page_id;  // Leaf: left sibling. Internal: leftmost child page id.
    std::uint16_t slot_count;
    std::uint16_t free_ptr;  // Lowest offset used by record heap (records live in [free_ptr, page_size)).
    std::uint8_t is_leaf;
};
struct Slot {
    std::uint16_t offset;  // Byte offset from page start to record.
    std::uint16_t size;    // Record length in bytes.
};
#pragma pack(pop)


/** Values persisted in leaf pages must serialize to a byte string via this interface. */
class IBTreeValue {
public:
    virtual ~IBTreeValue() = default;
    virtual std::string toStoreString() const = 0;
};

class BTreeStore {
public:
    /** Reserved tombstone payload; keys with this exact serialization are treated as deleted. */
    static const std::string kTombstonePayload;

    explicit BTreeStore(std::filesystem::path dataDirectory, double splitOccupancy = 0.88,
                        double mergeOccupancy = 0.28);
    ~BTreeStore();

    BTreeStore(const BTreeStore&) = delete;
    BTreeStore& operator=(const BTreeStore&) = delete;

    void insert(std::uint32_t key, std::unique_ptr<IBTreeValue> value);
    void remove(std::uint32_t key);

    /** Absent if missing or logically deleted (tombstone). */
    std::optional<std::string> search(std::uint32_t key) const;

    std::vector<std::pair<std::uint32_t, std::string>> rangeQuery(std::uint32_t startKey,
                                                                   std::uint32_t endKey) const;

private:
    struct SplitResult {
        bool has_split = false;
        std::uint32_t separator_key = 0;
        std::uint32_t right_page_id = 0;
    };

    std::filesystem::path storePath_;
    double split_occ_;
    double merge_occ_;

    mutable std::fstream file_;

    void openOrCreateFile();
    void readSuper(std::uint32_t& root_page_id, std::uint32_t& highest_page_id) const;
    void writeSuper(std::uint32_t root_page_id, std::uint32_t highest_page_id) const;

    /** Extends the backing file by one page; returns the new page id. */
    std::uint32_t allocatePageId(bool is_leaf);
    void setChildrenParent(std::uint32_t parent_page_id, const std::uint8_t* page);
    void writePage(std::uint32_t page_id, const std::uint8_t* data) const;
    void readPage(std::uint32_t page_id, std::uint8_t* data) const;

    static PageHeader* header(std::uint8_t* page);
    static const PageHeader* header(const std::uint8_t* page);

    SplitResult insertRecursive(std::uint32_t page_id, std::uint32_t key, const std::string& payload,
                                std::uint32_t& root_page_id, std::uint32_t& highest_page_id);

    std::uint32_t findLeafPage(std::uint32_t root_page_id, std::uint32_t key) const;
    std::uint32_t leftmostLeafPage(std::uint32_t root_page_id) const;

    bool maybeMergeAfterRemove(std::uint32_t leaf_page_id, std::uint32_t& root_page_id,
                               std::uint32_t highest_page_id);
};

#endif
