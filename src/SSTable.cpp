#include "../h/SSTable.hpp"

#include <cstring>

const char SSTable::kMagic[4] = {'S', 'S', 'T', '1'};
#include <fstream>
#include <stdexcept>

bool SSTable::write(const std::filesystem::path& path, const std::vector<Entry>& entries) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        return false;
    }
    out.write(kMagic, sizeof(kMagic));
    std::uint32_t ver = kVersion;
    out.write(reinterpret_cast<const char*>(&ver), sizeof(ver));
    std::uint32_t count = static_cast<std::uint32_t>(entries.size());
    out.write(reinterpret_cast<const char*>(&count), sizeof(count));
    for (const auto& e : entries) {
        std::int32_t k = static_cast<std::int32_t>(e.first);
        std::int32_t v = static_cast<std::int32_t>(e.second);
        out.write(reinterpret_cast<const char*>(&k), sizeof(k));
        out.write(reinterpret_cast<const char*>(&v), sizeof(v));
    }
    return static_cast<bool>(out);
}

std::vector<SSTable::Entry> SSTable::readAll(const std::filesystem::path& path) {
    std::vector<Entry> out;
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return out;
    }
    char magic[4];
    in.read(magic, sizeof(magic));
    if (in.gcount() != 4 || std::memcmp(magic, kMagic, 4) != 0) {
        return {};
    }
    std::uint32_t ver = 0;
    in.read(reinterpret_cast<char*>(&ver), sizeof(ver));
    if (!in || ver != kVersion) {
        return {};
    }
    std::uint32_t count = 0;
    in.read(reinterpret_cast<char*>(&count), sizeof(count));
    if (!in) {
        return {};
    }
    out.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        std::int32_t k = 0;
        std::int32_t v = 0;
        in.read(reinterpret_cast<char*>(&k), sizeof(k));
        in.read(reinterpret_cast<char*>(&v), sizeof(v));
        if (!in) {
            return {};
        }
        out.emplace_back(static_cast<int>(k), static_cast<int>(v));
    }
    return out;
}

bool SSTable::searchKey(const std::filesystem::path& path, int key, int& outValue) {
    auto entries = readAll(path);
    for (const auto& e : entries) {
        if (e.first == key) {
            outValue = e.second;
            return true;
        }
    }
    return false;
}
