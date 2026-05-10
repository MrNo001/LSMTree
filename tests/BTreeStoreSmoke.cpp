#include "../h/BTreeStore.hpp"

#include <cassert>
#include <cstring>
#include <filesystem>
#include <memory>
#include <utility>

namespace {

class IntPayload final : public IBTreeValue {
    int v_;

public:
    explicit IntPayload(int v) : v_(v) {}

    std::string toStoreString() const override {
        std::string s(sizeof(int), '\0');
        std::memcpy(s.data(), &v_, sizeof(v_));
        return s;
    }
};

int decodeInt(const std::optional<std::string>& o) {
    if (!o || o->size() < sizeof(int)) {
        return -1;
    }
    int v = 0;
    std::memcpy(&v, o->data(), sizeof(v));
    return v;
}

int decodePairValue(const std::pair<std::uint32_t, std::string>& p) {
    if (p.second.size() < sizeof(int)) {
        return -1;
    }
    int v = 0;
    std::memcpy(&v, p.second.data(), sizeof(v));
    return v;
}

}  // namespace

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
        BTreeStore store(tmp, 0.88, 0.28);
        store.insert(1u, std::make_unique<IntPayload>(10));
        store.insert(2u, std::make_unique<IntPayload>(20));
        store.insert(3u, std::make_unique<IntPayload>(30));
        assert(decodeInt(store.search(2u)) == 20);
    }

    {
        BTreeStore store(tmp, 0.88, 0.28);
        assert(decodeInt(store.search(1u)) == 10);
        assert(decodeInt(store.search(2u)) == 20);
        assert(decodeInt(store.search(3u)) == 30);
    }

    {
        resetStoreDir();
        BTreeStore store(tmp, 0.88, 0.28);
        store.insert(7u, std::make_unique<IntPayload>(70));
        store.remove(7u);
        assert(!store.search(7u).has_value());
        store.insert(7u, std::make_unique<IntPayload>(71));
        assert(decodeInt(store.search(7u)) == 71);
    }

    {
        resetStoreDir();
        BTreeStore store(tmp, 0.88, 0.28);
        store.insert(10u, std::make_unique<IntPayload>(100));
        store.insert(4u, std::make_unique<IntPayload>(40));
        store.insert(8u, std::make_unique<IntPayload>(80));
        store.remove(4u);
        const auto result = store.rangeQuery(1u, 10u);
        assert(result.size() == 2u);
        assert(result[0].first == 8u);
        assert(decodePairValue(result[0]) == 80);
        assert(result[1].first == 10u);
        assert(decodePairValue(result[1]) == 100);
    }

    //fs::remove_all(tmp);
    return 0;
}
