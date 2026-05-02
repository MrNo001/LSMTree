#include "../h/BTreeStore.hpp"

#include <algorithm>
#include <fstream>
#include <string>

namespace {

std::size_t clampOrder(std::size_t requested) {
    return std::max<std::size_t>(requested, 3);
}

}  // namespace

BTreeStore::BTreeStore(std::filesystem::path dataDirectory, std::size_t nodeOrder)
    : dataDir_(std::move(dataDirectory)),
      storePath_(dataDir_ / "btree.db"),
      order_(clampOrder(nodeOrder)),
      root_(new Node()) {
    std::filesystem::create_directories(dataDir_);
    loadFromDisk();
}

BTreeStore::~BTreeStore() {
    destroy(root_);
}

void BTreeStore::destroy(Node* node) {
    if (node == nullptr) {
        return;
    }
    for (Node* child : node->children) {
        destroy(child);
    }
    delete node;
}

BTreeStore::SplitResult BTreeStore::splitLeaf(Node* leaf) {
    auto* right = new Node();
    right->isLeaf = true;

    const std::size_t splitAt = leaf->keys.size() / 2;
    right->keys.assign(leaf->keys.begin() + static_cast<std::ptrdiff_t>(splitAt), leaf->keys.end());
    right->values.assign(leaf->values.begin() + static_cast<std::ptrdiff_t>(splitAt), leaf->values.end());

    leaf->keys.erase(leaf->keys.begin() + static_cast<std::ptrdiff_t>(splitAt), leaf->keys.end());
    leaf->values.erase(leaf->values.begin() + static_cast<std::ptrdiff_t>(splitAt), leaf->values.end());

    right->nextLeaf = leaf->nextLeaf;
    leaf->nextLeaf = right;

    SplitResult result;
    result.hasSplit = true;
    result.separatorKey = right->keys.front();
    result.rightNode = right;
    return result;
}

BTreeStore::SplitResult BTreeStore::splitInternal(Node* internal) {
    auto* right = new Node();
    right->isLeaf = false;

    const std::size_t middle = internal->keys.size() / 2;
    const int separator = internal->keys[middle];

    right->keys.assign(
        internal->keys.begin() + static_cast<std::ptrdiff_t>(middle + 1),
        internal->keys.end());
    internal->keys.erase(
        internal->keys.begin() + static_cast<std::ptrdiff_t>(middle),
        internal->keys.end());

    right->children.assign(
        internal->children.begin() + static_cast<std::ptrdiff_t>(middle + 1),
        internal->children.end());
    internal->children.erase(
        internal->children.begin() + static_cast<std::ptrdiff_t>(middle + 1),
        internal->children.end());

    SplitResult result;
    result.hasSplit = true;
    result.separatorKey = separator;
    result.rightNode = right;
    return result;
}

BTreeStore::SplitResult BTreeStore::insertRecursive(Node* node, int key, int value) {
    if (node->isLeaf) {
        auto it = std::lower_bound(node->keys.begin(), node->keys.end(), key);
        const std::size_t idx = static_cast<std::size_t>(it - node->keys.begin());
        if (it != node->keys.end() && *it == key) {
            node->values[idx] = value;
            return {};
        }
        node->keys.insert(it, key);
        node->values.insert(node->values.begin() + static_cast<std::ptrdiff_t>(idx), value);
        if (node->keys.size() < order_) {
            return {};
        }
        return splitLeaf(node);
    }

    auto it = std::upper_bound(node->keys.begin(), node->keys.end(), key);
    const std::size_t childIdx = static_cast<std::size_t>(it - node->keys.begin());
    SplitResult childSplit = insertRecursive(node->children[childIdx], key, value);
    if (!childSplit.hasSplit) {
        return {};
    }

    node->keys.insert(
        node->keys.begin() + static_cast<std::ptrdiff_t>(childIdx),
        childSplit.separatorKey);
    node->children.insert(
        node->children.begin() + static_cast<std::ptrdiff_t>(childIdx + 1),
        childSplit.rightNode);

    if (node->keys.size() < order_) {
        return {};
    }
    return splitInternal(node);
}

void BTreeStore::insert(int key, int value) {
    SplitResult split = insertRecursive(root_, key, value);
    if (split.hasSplit) {
        auto* newRoot = new Node();
        newRoot->isLeaf = false;
        newRoot->keys.push_back(split.separatorKey);
        newRoot->children.push_back(root_);
        newRoot->children.push_back(split.rightNode);
        root_ = newRoot;
    }
    persistToDisk();
}

void BTreeStore::remove(int key) {
    insert(key, kTombstoneValue);
}

const BTreeStore::Node* BTreeStore::findLeaf(int key) const {
    const Node* node = root_;
    while (node != nullptr && !node->isLeaf) {
        auto it = std::upper_bound(node->keys.begin(), node->keys.end(), key);
        const std::size_t childIdx = static_cast<std::size_t>(it - node->keys.begin());
        node = node->children[childIdx];
    }
    return node;
}

int BTreeStore::search(int key) const {
    const Node* leaf = findLeaf(key);
    if (leaf == nullptr) {
        return -1;
    }
    auto it = std::lower_bound(leaf->keys.begin(), leaf->keys.end(), key);
    if (it == leaf->keys.end() || *it != key) {
        return -1;
    }
    const std::size_t idx = static_cast<std::size_t>(it - leaf->keys.begin());
    const int value = leaf->values[idx];
    return value == kTombstoneValue ? -1 : value;
}

BTreeStore::Node* BTreeStore::leftmostLeaf() const {
    Node* node = root_;
    while (node != nullptr && !node->isLeaf) {
        node = node->children.front();
    }
    return node;
}

std::vector<std::pair<int, int>> BTreeStore::rangeQuery(int startKey, int endKey) const {
    if (startKey > endKey) {
        std::swap(startKey, endKey);
    }

    std::vector<std::pair<int, int>> out;
    const Node* leaf = findLeaf(startKey);
    if (leaf == nullptr) {
        leaf = leftmostLeaf();
    }

    while (leaf != nullptr) {
        for (std::size_t i = 0; i < leaf->keys.size(); ++i) {
            const int key = leaf->keys[i];
            if (key < startKey) {
                continue;
            }
            if (key > endKey) {
                return out;
            }
            const int value = leaf->values[i];
            if (value != kTombstoneValue) {
                out.emplace_back(key, value);
            }
        }
        leaf = leaf->nextLeaf;
    }
    return out;
}

void BTreeStore::persistToDisk() const {
    const std::filesystem::path tmpPath = storePath_.string() + ".tmp";
    std::ofstream out(tmpPath, std::ios::trunc);
    if (!out.is_open()) {
        return;
    }
    const Node* leaf = leftmostLeaf();
    while (leaf != nullptr) {
        for (std::size_t i = 0; i < leaf->keys.size(); ++i) {
            if (leaf->values[i] == kTombstoneValue) {
                continue;
            }
            out << leaf->keys[i] << ' ' << leaf->values[i] << '\n';
        }
        leaf = leaf->nextLeaf;
    }
    out.close();
    std::error_code ec;
    std::filesystem::remove(storePath_, ec);
    std::filesystem::rename(tmpPath, storePath_, ec);
}

void BTreeStore::loadFromDisk() {
    if (!std::filesystem::exists(storePath_)) {
        return;
    }
    std::ifstream in(storePath_);
    if (!in.is_open()) {
        return;
    }

    int key = 0;
    int value = 0;
    while (in >> key >> value) {
        SplitResult split = insertRecursive(root_, key, value);
        if (split.hasSplit) {
            auto* newRoot = new Node();
            newRoot->isLeaf = false;
            newRoot->keys.push_back(split.separatorKey);
            newRoot->children.push_back(root_);
            newRoot->children.push_back(split.rightNode);
            root_ = newRoot;
        }
    }
}
