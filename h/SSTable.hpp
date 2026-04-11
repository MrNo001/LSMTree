#ifndef SSTABLE_HPP
#define SSTABLE_HPP

#include <cstdint>
#include <filesystem>
#include <utility>
#include <vector>

class SSTable {
public:
    using Entry = std::pair<int, int>;

    static bool write(const std::filesystem::path& path, const std::vector<Entry>& entries);
    static std::vector<Entry> readAll(const std::filesystem::path& path);
    static bool searchKey(const std::filesystem::path& path, int key, int& outValue);

private:
    static const char kMagic[4];
    static constexpr std::uint32_t kVersion = 1;
};

#endif
