#include "../h/LSMStore.hpp"

#include "../h/Manifest.hpp"
#include "../h/SSTable.hpp"

#include <algorithm>
#include <iomanip>
#include <regex>
#include <sstream>

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

}  // namespace

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

int LSMStore::search(int key) const {
    const int inMem = memtable_->search(key);
    if (inMem != -1) {
        return inMem;
    }
    for (auto it = sstablePaths_.rbegin(); it != sstablePaths_.rend(); ++it) {
        int value = 0;
        if (SSTable::searchKey(absolutePath(*it), key, value)) {
            return value;
        }
    }
    return -1;
}

std::size_t LSMStore::memtableSize() const {
    return static_cast<std::size_t>(memtable_->getSize());
}

const std::vector<std::string>& LSMStore::sstableRelativePaths() const {
    return sstablePaths_;
}
