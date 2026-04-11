#include "../h/Manifest.hpp"

#include <fstream>
#include <sstream>

namespace {

bool isBlankOrComment(const std::string& line) {
    for (char c : line) {
        if (c == '#') {
            return true;
        }
        if (c != ' ' && c != '\t' && c != '\r') {
            return false;
        }
    }
    return true;
}

}  // namespace

bool Manifest::load(const std::filesystem::path& path, std::vector<std::string>& outRelativePaths) {
    outRelativePaths.clear();
    std::ifstream in(path);
    if (!in) {
        return true;
    }
    std::string line;
    while (std::getline(in, line)) {
        if (isBlankOrComment(line)) {
            continue;
        }
        std::istringstream iss(line);
        std::string rel;
        iss >> rel;
        if (!rel.empty()) {
            outRelativePaths.push_back(std::move(rel));
        }
    }
    return true;
}

bool Manifest::save(const std::filesystem::path& path, const std::vector<std::string>& relativePaths) {
    std::ofstream out(path, std::ios::trunc);
    if (!out) {
        return false;
    }
    out << "# LSM manifest — one SST path per line (relative to data directory)\n";
    for (const auto& rel : relativePaths) {
        out << rel << '\n';
    }
    return static_cast<bool>(out);
}
