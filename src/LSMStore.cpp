#include "../h/LSMStore.hpp"

#include "../h/Manifest.hpp"
#include "../h/SSTable.hpp"

#include <algorithm>
#include <iomanip>
#include <map>
#include <regex>
#include <sstream>
#include <unordered_set>

namespace {

std::uint64_t parseSstIdFromFilename(const std::string& filename) {
    static const std::regex re(R"(sst_(\d+)\.sst)", std::regex::ECMAScript);
    std::smatch m;
    if (std::regex_search(filename, m, re) && m.size() >= 2) {
        return static_cast<std::uint64_t>(std::stoull(m[1].str()));
    }
    return 0;
}

std::string makeSstFilename(std::uint64_t id) {
    std::ostringstream oss;
    oss << "sst_" << std::setw(6) << std::setfill('0') << id << ".sst";
    return oss.str();
}

}  // helpers 

LSMStore::LSMStore(std::filesystem::path dataDirectory, std::size_t memtableFlushThresholdEntries)
    : dataDir_(std::move(dataDirectory)),
      manifestPath_(dataDir_ / "MANIFEST"),
      flushThreshold_(memtableFlushThresholdEntries),
      memtable_(std::make_unique<SkipList>()) {
    std::filesystem::create_directories(dataDir_);
    loadManifest();
}

void LSMStore::loadManifest() {
    Manifest::load(manifestPath_, sstablePaths_);
}

void LSMStore::saveManifest() const {
    Manifest::save(manifestPath_, sstablePaths_);
}

std::filesystem::path LSMStore::absolutePath(const std::string& relative) const {
    return dataDir_ / relative;
}

std::uint64_t LSMStore::nextSstId() const {
    std::uint64_t maxId = 0;
    for (const auto& rel : sstablePaths_) {
        const std::filesystem::path p(rel);
        maxId = std::max(maxId, parseSstIdFromFilename(p.filename().string()));
    }
    return maxId + 1;
}

void LSMStore::flushMemtable() {
    auto entries = memtable_->sortedEntries();
    if (entries.empty()) {
        return;
    }
    const std::uint64_t id = nextSstId();
    const std::string rel = makeSstFilename(id);
    const std::filesystem::path outPath = absolutePath(rel);
    if (!SSTable::write(outPath, entries)) {
        return;
    }
    sstablePaths_.push_back(rel);
    saveManifest();
    memtable_->clear();
}

void LSMStore::flushMemtableIfNeeded() {
    if (flushThreshold_ > 0 && static_cast<std::size_t>(memtable_->getSize()) >= flushThreshold_) {
        flushMemtable();
    }
}

void LSMStore::insert(int key, int value) {
    memtable_->insert(key, value);
    flushMemtableIfNeeded();
}

void LSMStore::remove(int key) {
    memtable_->insert(key, kTombstoneValue);
    flushMemtableIfNeeded();
}

int LSMStore::search(int key) const {
    const int inMem = memtable_->search(key);
    if (inMem != -1) {
        if (inMem == kTombstoneValue) {
            return -1;
        }
        return inMem;
    }
    for (auto it = sstablePaths_.rbegin(); it != sstablePaths_.rend(); ++it) {
        int value = 0;
        if (SSTable::searchKey(absolutePath(*it), key, value)) {
            if (value == kTombstoneValue) {
                return -1;
            }
            return value;
        }
    }
    return -1;
}

std::vector<std::pair<int, int>> LSMStore::rangeQuery(int startKey, int endKey) const {
    if (startKey > endKey) {
        std::swap(startKey, endKey);
    }

    // Keep the newest value per key while maintaining sorted output by key.
    std::map<int, int> resolved;
    std::unordered_set<int> seen;

    const auto considerEntry = [&](int key, int value) {
        if (key < startKey || key > endKey) {
            return;
        }
        if (seen.find(key) != seen.end()) {
            return;
        }
        seen.insert(key);
        resolved[key] = value;
    };

    for (const auto& entry : memtable_->sortedEntries()) {
        considerEntry(entry.first, entry.second);
    }

    for (auto it = sstablePaths_.rbegin(); it != sstablePaths_.rend(); ++it) {
        for (const auto& entry : SSTable::readAll(absolutePath(*it))) {
            considerEntry(entry.first, entry.second);
        }
    }

    std::vector<std::pair<int, int>> out;
    out.reserve(resolved.size());
    for (const auto& kv : resolved) {
        if (kv.second != kTombstoneValue) {
            out.push_back(kv);
        }
    }
    return out;
}

std::size_t LSMStore::memtableSize() const {
    return static_cast<std::size_t>(memtable_->getSize());
}

const std::vector<std::string>& LSMStore::sstableRelativePaths() const {
    return sstablePaths_;
}
