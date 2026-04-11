#ifndef LSMSTORE_HPP
#define LSMSTORE_HPP

#include "SkipList.hpp"

#include <cstddef>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

class LSMStore {
public:
    LSMStore(std::filesystem::path dataDirectory, std::size_t memtableFlushThresholdEntries);

    void insert(int key, int value);
    int search(int key) const;

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
