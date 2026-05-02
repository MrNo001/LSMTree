#ifndef BTREESTORE_HPP
#define BTREESTORE_HPP

#include <cstddef>
#include <filesystem>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

class BTreeStore {
public:
    static constexpr int kTombstoneValue = std::numeric_limits<int>::min();

    explicit BTreeStore(std::filesystem::path dataDirectory, std::size_t nodeOrder = 32);
    ~BTreeStore();

    void insert(int key, int value);
    void remove(int key);
    int search(int key) const;
    std::vector<std::pair<int, int>> rangeQuery(int startKey, int endKey) const;

private:
    struct Node {
        bool isLeaf = true;
        std::vector<int> keys;
        std::vector<int> values;
        std::vector<Node*> children;
        Node* nextLeaf = nullptr;
    };

    struct SplitResult {
        bool hasSplit = false;
        int separatorKey = 0;
        Node* rightNode = nullptr;
    };

    SplitResult insertRecursive(Node* node, int key, int value);
    SplitResult splitLeaf(Node* leaf);
    SplitResult splitInternal(Node* internal);
    const Node* findLeaf(int key) const;
    Node* leftmostLeaf() const;
    void destroy(Node* node);

    void loadFromDisk();
    void persistToDisk() const;

    std::filesystem::path dataDir_;
    std::filesystem::path storePath_;
    std::size_t order_;
    Node* root_;
};

#endif
