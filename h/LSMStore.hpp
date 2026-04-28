#ifndef LSMSTORE_HPP
#define LSMSTORE_HPP

#include "SkipList.hpp"

#include <cstddef>
#include <filesystem>
#include <limits>
#include <memory>
#include <string>
#include <vector>

class LSMStore {
public:
    /** Reserved value; do not use as a logical payload in insert(). */
    static constexpr int kTombstoneValue = std::numeric_limits<int>::min();

    LSMStore(std::filesystem::path dataDirectory, std::size_t memtableFlushThresholdEntries);

    void insert(int key, int value);
    void remove(int key);
    int search(int key) const;
    std::vector<std::pair<int, int>> rangeQuery(int startKey, int endKey) const;

    std::size_t memtableSize() const;
    const std::vector<std::string>& sstableRelativePaths() const;

private:
    void loadManifest();
    void saveManifest() const;
    void flushMemtableIfNeeded();
    void flushMemtable();
    std::uint64_t nextSstId() const;
    std::filesystem::path absolutePath(const std::string& relative) const;

    std::filesystem::path dataDir_;
    std::filesystem::path manifestPath_;
    std::size_t flushThreshold_;
    std::unique_ptr<SkipList> memtable_;
    std::vector<std::string> sstablePaths_;
};

#endif
