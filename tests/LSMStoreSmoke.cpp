#include "../h/LSMStore.hpp"

#include <cassert>
#include <filesystem>
#include <utility>

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

    {
        LSMStore store(tmp, 100);
        store.insert(7, 70);
        store.remove(7);
        assert(store.search(7) == -1);
        store.insert(7, 71);
        assert(store.search(7) == 71);
    }

    {
        fs::remove_all(tmp);
        fs::create_directories(tmp);
        LSMStore store(tmp, 3);
        store.insert(1, 10);
        store.insert(2, 20);
        store.insert(3, 30);
        assert(store.sstableRelativePaths().size() == 1);
        store.remove(1);
        store.insert(4, 40);
        store.insert(5, 50);
        assert(store.sstableRelativePaths().size() == 2);
        LSMStore store2(tmp, 100);
        assert(store2.search(1) == -1);
        assert(store2.search(2) == 20);
    }

    {
        fs::remove_all(tmp);
        fs::create_directories(tmp);
        LSMStore store(tmp, 2);
        store.insert(1, 10);
        store.insert(3, 30);  // flush #1
        store.insert(2, 20);
        store.insert(3, 31);  // flush #2 (newer value for key 3)
        store.remove(1);
        store.insert(4, 40);  // flush #3 (tombstone for key 1)
        store.insert(5, 50);  // remains in memtable

        const auto result = store.rangeQuery(1, 5);
        assert(result.size() == 4);
        assert(result[0] == std::make_pair(2, 20));
        assert(result[1] == std::make_pair(3, 31));
        assert(result[2] == std::make_pair(4, 40));
        assert(result[3] == std::make_pair(5, 50));
    }

    {
        fs::remove_all(tmp);
        fs::create_directories(tmp);
        LSMStore store(tmp, 10);
        store.insert(1, 100);
        store.insert(4, 400);
        const auto result = store.rangeQuery(5, 2);
        assert(result.size() == 1);
        assert(result[0] == std::make_pair(4, 400));
    }

    fs::remove_all(tmp);
    return 0;
}
