#ifndef MANIFEST_HPP
#define MANIFEST_HPP

#include <filesystem>
#include <string>
#include <vector>

class Manifest {
public:
    static bool load(const std::filesystem::path& path, std::vector<std::string>& outRelativePaths);
    static bool save(const std::filesystem::path& path, const std::vector<std::string>& relativePaths);
};

#endif
