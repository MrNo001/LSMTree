#include "../h/LSMStore.hpp"

#include <cassert>
#include <filesystem>

int main() {
    namespace fs = std::filesystem;
    // Project root = parent of tests/ (works no matter what cwd is when you run the binary)
    const fs::path projectRoot = fs::path(__FILE__).parent_path().parent_path();
    const fs::path tmp = projectRoot / "lsm_store_smoke";
    fs::remove_all(tmp);
    fs::create_directories(tmp);

    {
        LSMStore store(tmp, 3);
        store.insert(1, 10);
        store.insert(2, 20);
        assert(store.search(1) == 10);
        store.insert(3, 30);
        assert(store.memtableSize() == 0);
        assert(store.sstableRelativePaths().size() == 1);
    }
    {
        LSMStore store2(tmp, 100);
        assert(store2.search(1) == 10);
        assert(store2.search(2) == 20);
        assert(store2.search(3) == 30);
    }

    fs::remove_all(tmp);
    return 0;
}
