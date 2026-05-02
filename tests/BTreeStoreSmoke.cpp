#include "../h/BTreeStore.hpp"

#include <cassert>
#include <filesystem>
#include <utility>

int main() {
    namespace fs = std::filesystem;
    const fs::path projectRoot = fs::path(__FILE__).parent_path().parent_path();
    const fs::path tmp = projectRoot / "btree_store_smoke";
    const auto resetStoreDir = [&]() {
        fs::remove_all(tmp);
        fs::create_directories(tmp);
    };

    {
        resetStoreDir();
        BTreeStore store(tmp, 4);
        store.insert(1, 10);
        store.insert(2, 20);
        store.insert(3, 30);
        assert(store.search(2) == 20);
    }

    {
        BTreeStore store(tmp, 4);
        assert(store.search(1) == 10);
        assert(store.search(2) == 20);
        assert(store.search(3) == 30);
    }

    {
        resetStoreDir();
        BTreeStore store(tmp, 4);
        store.insert(7, 70);
        store.remove(7);
        assert(store.search(7) == -1);
        store.insert(7, 71);
        assert(store.search(7) == 71);
    }

    {
        resetStoreDir();
        BTreeStore store(tmp, 4);
        store.insert(10, 100);
        store.insert(4, 40);
        store.insert(8, 80);
        store.remove(4);
        const auto result = store.rangeQuery(1, 10);
        assert(result.size() == 2);
        assert(result[0] == std::make_pair(8, 80));
        assert(result[1] == std::make_pair(10, 100));
    }

    //fs::remove_all(tmp);
    return 0;
}
