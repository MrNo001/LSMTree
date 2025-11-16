#ifndef BPLUSTREE_H
#define BPLUSTREE_H

#include <cstdlib>
#include <vector>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <cmath>
#include <utility> 
#include <string>

using namespace std;

template <typename T>
class BPlusTree {
public:
    using ValueType = int;
    using Record = pair<T, ValueType>;

    struct Node {
        bool isLeaf;
        vector<T> keys;
        vector<ValueType> values;
        vector<Node*> children;
        Node* next; 

        Node(bool leaf = false)
            : isLeaf(leaf), next(nullptr) {
        }
    };

   
    struct PageNode {
        bool isLeaf;
        vector<T> keys;
        vector<ValueType> values; 
        vector<int> childrenPages;
        int nextLeafPage;

        PageNode(bool leaf = false)
            : isLeaf(leaf), nextLeafPage(-1) {
        }
    };

    const int PAGE_SIZE = 4096;
    int isLeafValue = 33686018;
    int isNotLeafValue = 16843009;

    int firstFreePage = 0;
    vector<int>* childrenLocation = new vector<int>();

    Node* root;
    int t; 
    int numOfElements = 0;

private:
    void splitChild(Node* parent, int index, Node* child);
    void insertNonFull(Node* node, T key, ValueType value);
    void remove(Node* node, T key);
    void borrowFromPrev(Node* node, int index);
    void borrowFromNext(Node* node, int index);
    void merge(Node* node, int index);
    void printTree(Node* node, int level);
    void writeToFile(Node* node, fstream& dbFile, int page);
    PageNode* pageToNode(fstream& dbFile, int page);

public:
    BPlusTree(int degree) : root(nullptr), t(degree) {}
    ~BPlusTree(); 

    void insert(T key, ValueType value);
    ValueType* search(T key);
    void remove(T key);
    vector<Record> rangeQuery(T lower, T upper);
    vector<Record> getAllRecords();
    void printTree();
    void bottom_up(const vector<Record>& sorted_data);

    bool writeToFile(string filename, int page_size = 4096);
    vector<Record>* getFromFile(string Filename);
};

template <typename T>
void BPlusTree<T>::insert(T key, ValueType value) {
    if (root == nullptr) {
        root = new Node(true);
        root->keys.push_back(key);
        root->values.push_back(value);
    }
    else {
        if (root->keys.size() == 2 * t - 1) {
            Node* newRoot = new Node(false);
            newRoot->children.push_back(root);
            splitChild(newRoot, 0, root);
            root = newRoot;
        }
        insertNonFull(root, key, value);
    }
    numOfElements++;
}

template <typename T>
void BPlusTree<T>::insertNonFull(Node* node, T key, ValueType value) {
    if (node->isLeaf) {
     
        auto it = upper_bound(node->keys.begin(), node->keys.end(), key);
        int pos = distance(node->keys.begin(), it);
        node->keys.insert(it, key);
        node->values.insert(node->values.begin() + pos, value);
    }
    else {
        int i = upper_bound(node->keys.begin(), node->keys.end(), key) - node->keys.begin();

        
        if (node->children[i]->keys.size() == 2 * t - 1) {
            splitChild(node, i, node->children[i]);
      
            if (key > node->keys[i]) {
                i++;
            }
        }
        insertNonFull(node->children[i], key, value);
    }
}


template <typename T>
void BPlusTree<T>::splitChild(Node* parent, int index, Node* child) {
    Node* newSibling = new Node(child->isLeaf);

    if (child->isLeaf) {

        newSibling->keys.assign(child->keys.begin() + t, child->keys.end());
        newSibling->values.assign(child->values.begin() + t, child->values.end());

        T keyToPromote = newSibling->keys[0];

        child->keys.resize(t);
        child->values.resize(t);

        newSibling->next = child->next;
        child->next = newSibling;

        parent->keys.insert(parent->keys.begin() + index, keyToPromote);
        parent->children.insert(parent->children.begin() + index + 1, newSibling);

    }
    else {
        T keyToPromote = child->keys[t - 1];

        newSibling->keys.assign(child->keys.begin() + t, child->keys.end());

        newSibling->children.assign(child->children.begin() + t, child->children.end());

        child->keys.resize(t - 1);
        child->children.resize(t);

        parent->keys.insert(parent->keys.begin() + index, keyToPromote);
        parent->children.insert(parent->children.begin() + index + 1, newSibling);
    }
}


template <typename T>
typename BPlusTree<T>::ValueType* BPlusTree<T>::search(T key) {
    if (root == nullptr) return nullptr;
    Node* current = root;
    while (!current->isLeaf) {
        auto it = lower_bound(current->keys.begin(), current->keys.end(), key);
        int i = distance(current->keys.begin(), it);
        if (it != current->keys.end() && *it == key) {
            current = current->children[i + 1];
        }
        else {
            current = current->children[i];
        }
    }
    auto it = lower_bound(current->keys.begin(), current->keys.end(), key);
    if (it != current->keys.end() && *it == key) {
        int pos = distance(current->keys.begin(), it);
        return &current->values[pos];
    }
    return nullptr;
}

template <typename T>
void BPlusTree<T>::remove(T key) {
    if (!root) return;
    remove(root, key);
    if (root->keys.empty() && !root->isLeaf) {
        Node* oldRoot = root;
        root = root->children[0];
        delete oldRoot;
    }
}

template <typename T>
void BPlusTree<T>::remove(Node* node, T key) {
    if (node->isLeaf) {
        auto it = find(node->keys.begin(), node->keys.end(), key);
        if (it != node->keys.end()) {
            int pos = distance(node->keys.begin(), it);
            node->keys.erase(it);
            node->values.erase(node->values.begin() + pos);
            numOfElements--;
        }
    }
    else {
        int idx = lower_bound(node->keys.begin(), node->keys.end(), key) - node->keys.begin();
        
        if (idx < node->keys.size() && node->keys[idx] == key) {
            Node* leafNode = node->children[idx + 1];
            while (!leafNode->isLeaf) {
                leafNode = leafNode->children[0]; 
            }
            
            auto leafIt = find(leafNode->keys.begin(), leafNode->keys.end(), key);
            if (leafIt != leafNode->keys.end()) {
                int pos = distance(leafNode->keys.begin(), leafIt);
                leafNode->keys.erase(leafIt);
                leafNode->values.erase(leafNode->values.begin() + pos);
                numOfElements--;
                
                if (!leafNode->keys.empty()) {
                    node->keys[idx] = leafNode->keys[0];
                } else {
                    Node* predNode = node->children[idx];
                    while (!predNode->isLeaf) {
                        predNode = predNode->children.back();
                    }
                    if (!predNode->keys.empty()) {
                        node->keys[idx] = predNode->keys.back();
                    }
                }
            }
        }
        else {
            if (node->children[idx]->keys.size() < t) {
                if (idx > 0 && node->children[idx - 1]->keys.size() >= t) {
                    borrowFromPrev(node, idx);
                }
                else if (idx < node->children.size() - 1 && node->children[idx + 1]->keys.size() >= t) {
                    borrowFromNext(node, idx);
                }
                else {
                    if (idx < node->children.size() - 1) {
                        merge(node, idx);
                    }
                    else {
                        merge(node, idx - 1);
                        idx--; 
                    }
                }
            }
            remove(node->children[idx], key);
        }
    }
}

template <typename T>
void BPlusTree<T>::borrowFromPrev(Node* node, int index) {
    Node* child = node->children[index];
    Node* sibling = node->children[index - 1];

    if (child->isLeaf) {
        child->keys.insert(child->keys.begin(), sibling->keys.back());
        child->values.insert(child->values.begin(), sibling->values.back());
        sibling->keys.pop_back();
        sibling->values.pop_back();
        node->keys[index - 1] = child->keys[0];
    }
    else {
        child->keys.insert(child->keys.begin(), node->keys[index - 1]);
        node->keys[index - 1] = sibling->keys.back();
        sibling->keys.pop_back();
        
        child->children.insert(child->children.begin(), sibling->children.back());
        sibling->children.pop_back();
    }
}

template <typename T>
void BPlusTree<T>::borrowFromNext(Node* node, int index) {
    Node* child = node->children[index];
    Node* sibling = node->children[index + 1];

    if (child->isLeaf) {
        child->keys.push_back(sibling->keys.front());
        child->values.push_back(sibling->values.front());
        sibling->keys.erase(sibling->keys.begin());
        sibling->values.erase(sibling->values.begin());
        if (!sibling->keys.empty()) {
            node->keys[index] = sibling->keys.front();
        }
    }
    else {
        child->keys.push_back(node->keys[index]);
        node->keys[index] = sibling->keys.front();
        sibling->keys.erase(sibling->keys.begin());
        
        child->children.push_back(sibling->children.front());
        sibling->children.erase(sibling->children.begin());
    }
}

template <typename T>
void BPlusTree<T>::merge(Node* node, int index) {
    Node* child = node->children[index];
    Node* sibling = node->children[index + 1];

    if (child->isLeaf) {
        child->keys.insert(child->keys.end(), sibling->keys.begin(), sibling->keys.end());
        child->values.insert(child->values.end(), sibling->values.begin(), sibling->values.end());
        
        child->next = sibling->next;
    }
    else {
        child->keys.push_back(node->keys[index]);
        child->keys.insert(child->keys.end(), sibling->keys.begin(), sibling->keys.end());
        child->children.insert(child->children.end(), sibling->children.begin(), sibling->children.end());
    }

    node->keys.erase(node->keys.begin() + index);
    node->children.erase(node->children.begin() + index + 1);

    delete sibling;
}

template <typename T>
vector<typename BPlusTree<T>::Record> BPlusTree<T>::rangeQuery(T lower, T upper) {
    vector<Record> result;
    if (!root) return result;

    Node* current = root;
    while (!current->isLeaf) {
        int i = upper_bound(current->keys.begin(), current->keys.end(), lower) - current->keys.begin();
        current = current->children[i];
    }

    while (current != nullptr) {
        for (size_t i = 0; i < current->keys.size(); ++i) {
            if (current->keys[i] >= lower && current->keys[i] <= upper) {
                result.push_back({ current->keys[i], current->values[i] });
            }
            if (current->keys[i] > upper) {
                return result;
            }
        }
        current = current->next;
    }
    return result;
}

template <typename T>
vector<typename BPlusTree<T>::Record> BPlusTree<T>::getAllRecords() {
    vector<Record> result;
    if (!root) return result;

    Node* current = root;
    while (!current->isLeaf) {
        current = current->children[0];
    }

    while (current != nullptr) {
        for (size_t i = 0; i < current->keys.size(); ++i) {
            result.push_back({ current->keys[i], current->values[i] });
        }
        current = current->next;
    }
    return result;
}

// --- Utility, File I/O, and Bulk Load ---

template <typename T>
void BPlusTree<T>::printTree() {
    printTree(root, 0);
}

template <typename T>
void BPlusTree<T>::printTree(Node* node, int level) {
    if (node != nullptr) {
        for (int i = 0; i < level; ++i) cout << "  ";
        if (node->isLeaf) {
            cout << "[LEAF] Keys: ";
            for (size_t i = 0; i < node->keys.size(); ++i) {
                cout << node->keys[i] << ":" << node->values[i] << " ";
            }
        }
        else {
            cout << "[INTERNAL] Keys: ";
            for (const T& key : node->keys) cout << key << " ";
        }
        cout << endl;
        for (Node* child : node->children) {
            printTree(child, level + 1);
        }
    }
}

template <typename T>
bool BPlusTree<T>::writeToFile(string filename, int page_size) {
    fstream dbFile(filename, ios::binary | ios::in | ios::out | ios::trunc);
    if (!dbFile) { return false; }

    firstFreePage = 0;
    childrenLocation->clear();
    if (root) {
        writeToFile(root, dbFile, 0);
    }

    for (size_t i = 0; i < childrenLocation->size(); i++) {
        int page = childrenLocation->at(i);
        int nextPage = (i + 1 < childrenLocation->size()) ? childrenLocation->at(i + 1) : -1;
        dbFile.seekp(streampos(page * PAGE_SIZE + sizeof(int)));
        dbFile.write(reinterpret_cast<char*>(&nextPage), sizeof(nextPage));
    }
    dbFile.close();
    return true;
}

template <typename T>
void BPlusTree<T>::writeToFile(Node* node, fstream& dbFile, int page) {
    if (!node) return;
    dbFile.seekp(streampos(page * PAGE_SIZE));

    if (node->isLeaf) {
        dbFile.write(reinterpret_cast<char*>(&isLeafValue), sizeof(isLeafValue));
        int dummyNextPage = -1;
        dbFile.write(reinterpret_cast<char*>(&dummyNextPage), sizeof(dummyNextPage));
        childrenLocation->push_back(page);
    }
    else {
        dbFile.write(reinterpret_cast<char*>(&isNotLeafValue), sizeof(isNotLeafValue));
    }

    int keysSize = node->keys.size();
    dbFile.write(reinterpret_cast<const char*>(&keysSize), sizeof(keysSize));
    dbFile.write(reinterpret_cast<const char*>(node->keys.data()), keysSize * sizeof(T));

    if (node->isLeaf) {
        dbFile.write(reinterpret_cast<const char*>(node->values.data()), keysSize * sizeof(ValueType));
    }

    int childrenSize = node->children.size();
    dbFile.write(reinterpret_cast<char*>(&childrenSize), sizeof(childrenSize));

    vector<int> childPages;
    for (size_t i = 0; i < node->children.size(); ++i) {
        firstFreePage++;
        childPages.push_back(firstFreePage);
    }
    if (!childPages.empty()) {
        dbFile.write(reinterpret_cast<const char*>(childPages.data()), childPages.size() * sizeof(int));
    }

    for (size_t i = 0; i < node->children.size(); ++i) {
        writeToFile(node->children[i], dbFile, childPages[i]);
    }
}


template <typename T>
typename BPlusTree<T>::PageNode* BPlusTree<T>::pageToNode(fstream& dbFile, int page) {
    if (page < 0) return nullptr;

    PageNode* res = new PageNode();
    dbFile.seekg(streampos(page * PAGE_SIZE));
    int val;
    dbFile.read(reinterpret_cast<char*>(&val), sizeof(val));
    if (dbFile.gcount() == 0) { delete res; return nullptr; }

    if (val == isLeafValue) {
        res->isLeaf = true;
        dbFile.read(reinterpret_cast<char*>(&res->nextLeafPage), sizeof(res->nextLeafPage));
    }
    else {
        res->isLeaf = false;
    }

    int numberOfKeys = 0;
    dbFile.read(reinterpret_cast<char*>(&numberOfKeys), sizeof(numberOfKeys));
    
    if (numberOfKeys < 0 || numberOfKeys > 1000) { 
        delete res;
        return nullptr;
    }
    
    res->keys.resize(numberOfKeys);
    dbFile.read(reinterpret_cast<char*>(res->keys.data()), numberOfKeys * sizeof(T));

    if (res->isLeaf) {
        res->values.resize(numberOfKeys);
        dbFile.read(reinterpret_cast<char*>(res->values.data()), numberOfKeys * sizeof(ValueType));
    }

    int numberOfChildren = 0;
    dbFile.read(reinterpret_cast<char*>(&numberOfChildren), sizeof(numberOfChildren));
    
    if (numberOfChildren < 0 || numberOfChildren > 1000) { 
        delete res;
        return nullptr;
    }
    
    if (numberOfChildren > 0) {
        res->childrenPages.resize(numberOfChildren);
        dbFile.read(reinterpret_cast<char*>(res->childrenPages.data()), numberOfChildren * sizeof(int));
    }
    return res;
}

template <typename T>
vector<typename BPlusTree<T>::Record>* BPlusTree<T>::getFromFile(string Filename) {
    fstream dbFile(Filename, ios_base::binary | ios_base::in);
    if (!dbFile) { return nullptr; }

    vector<Record>* res = new vector<Record>();
    PageNode* current = pageToNode(dbFile, 0);
    if (!current) { delete res; return nullptr; }

    while (current && !current->isLeaf) {
        int nextPage = current->childrenPages.empty() ? -1 : current->childrenPages[0];
        delete current;
        current = pageToNode(dbFile, nextPage);
        
        if (current && current->childrenPages.empty() && !current->isLeaf) {
            delete current;
            current = nullptr;
        }
    }

    while (current) {
        for (size_t i = 0; i < current->keys.size(); ++i) {
            res->push_back({ current->keys[i], current->values[i] });
        }
        int nextPage = current->nextLeafPage;
        delete current;
        current = pageToNode(dbFile, nextPage);
        
        if (current && current->keys.empty()) {
            delete current;
            current = nullptr;
        }
    }
    dbFile.close();
    return res;
}

template<typename T>
void BPlusTree<T>::bottom_up(const vector<Record>& sorted_data) {
    if (sorted_data.empty()) {
        root = nullptr;
        return;
    }

    if (root) {
        root = nullptr;
    }

    vector<Node*> leaves;
    int n = sorted_data.size();
    int leaf_capacity = 2 * t - 1;

    for (int i = 0; i < n; i += leaf_capacity) {
        Node* leaf = new Node(true);
        int end = min(i + leaf_capacity, n);
        for (int j = i; j < end; j++) {
            leaf->keys.push_back(sorted_data[j].first);
            leaf->values.push_back(sorted_data[j].second);
        }
        leaves.push_back(leaf);
    }

    for (size_t i = 0; i < leaves.size() - 1; i++) {
        leaves[i]->next = leaves[i + 1];
    }

    vector<Node*> current_level = leaves;
    while (current_level.size() > 1) {
        vector<Node*> next_level;
        
        for (size_t i = 0; i < current_level.size(); i += (2 * t)) {
            Node* parent = new Node(false);
            size_t end = min(i + (2 * t), current_level.size());
            
            parent->children.push_back(current_level[i]);
            
            for (size_t j = i + 1; j < end; j++) {
                parent->keys.push_back(current_level[j]->keys[0]);
                parent->children.push_back(current_level[j]);
            }
            
            next_level.push_back(parent);
        }
        
        current_level = next_level;
    }

    root = current_level.empty() ? nullptr : current_level[0];
    numOfElements = n;
}

template <typename T>
BPlusTree<T>::~BPlusTree() {
    delete childrenLocation;
}

#endif // BPLUSTREE_H

