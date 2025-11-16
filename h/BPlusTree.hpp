

#ifndef BPLUSTREE_H
#define BPLUSTREE_H

#include <vector>
#include <fstream>
using namespace std;

// B plus tree class
template <typename T> class BPlusTree {
public:
    // structure to create a node
    struct Node {
        bool isLeaf;
        vector<T> keys;
        vector<int> values;
        vector<Node*> children;
        Node* next;

        Node(bool leaf = false)
            : isLeaf(leaf)
            , next(nullptr)
        {
        }
    };

    struct PageNode {
        bool isLeaf;
        vector<T> keys;
        vector<int> childrenPages;
        int nextLeafPage;
        PageNode(bool leaf = false)
            : isLeaf(leaf)
            , nextLeafPage(-1)
        {
        }

    };
    const int PAGE_SIZE = 4096;

     int isLeafValue =  33686018;
     int isNotLeafValue = 16843009;

    int firstFreePage = 0;

    Node* root;
    // Minimum degree (defines the range for the number of
    // keys)
    int t;

    // Function to split a child node
    void splitChild(Node* parent, int index, Node* child);

    // Function to insert a key in a non-full node
    void insertNonFull(Node* node, T key);

    // Function to remove a key from a node
    void remove(Node* node, T key);

    // Function to borrow a key from the previous sibling
    void borrowFromPrev(Node* node, int index);

    // Function to borrow a key from the next sibling
    void borrowFromNext(Node* node, int index);

    // Function to merge two nodes
    void merge(Node* node, int index);

    // Function to print the tree
    void printTree(Node* node, int level);

public:
    BPlusTree(int degree) : root(nullptr), t(degree) {}

    void insert(T key);
    bool search(T key);
    void remove(T key);
    vector<T> rangeQuery(T lower, T upper);
    void printTree();

    bool writeToFile(string filename,int page_size = 4096);
    //void writeToFile(Node* node,ofstream& dbFile,int page);
    void writeToFile(Node* node, fstream& dbFile,int page);
    vector<T>* getFromFile(string Filename);

    vector<T>* childrenLocation = new vector<T>();

    PageNode* pageToNode(fstream& dbFile,int page);

    Node* bottom_up(const vector<T>& sorted_keys);

    int numOfElements = 0;

};


template <typename T>
void BPlusTree<T>::splitChild(Node* parent, int index,
    Node* child)
{
    Node* newChild = new Node(child->isLeaf);
    parent->children.insert(
        parent->children.begin() + index + 1, newChild);
    parent->keys.insert(parent->keys.begin() + index,
        child->keys[t - 1]);

    newChild->keys.assign(child->keys.begin() + t,
        child->keys.end());
    child->keys.resize(t - 1);

    if (!child->isLeaf) {
        newChild->children.assign(child->children.begin()
            + t,
            child->children.end());
        child->children.resize(t);
    }

    if (child->isLeaf) {
        newChild->next = child->next;
        child->next = newChild;
    }
}

// Implementation of insertNonFull function
template <typename T>
void BPlusTree<T>::insertNonFull(Node* node, T key)
{
    if (node->isLeaf) {
        node->keys.insert(upper_bound(node->keys.begin(),
            node->keys.end(),
            key),
            key);
    }
    else {
        int i = node->keys.size() - 1;
        while (i >= 0 && key < node->keys[i]) {
            i--;
        }
        i++;
        if (node->children[i]->keys.size() == 2 * t - 1) {
            splitChild(node, i, node->children[i]);
            if (key > node->keys[i]) {
                i++;
            }
        }
        insertNonFull(node->children[i], key);
    }
}

// Implementation of remove function
template <typename T>
void BPlusTree<T>::remove(Node* node, T key)
{
    numOfElements--;
    // If node is a leaf
    if (node->isLeaf) {
        auto it = find(node->keys.begin(), node->keys.end(),
            key);
        if (it != node->keys.end()) {
            node->keys.erase(it);
        }
    }
    else {
        int idx = lower_bound(node->keys.begin(),
            node->keys.end(), key)
            - node->keys.begin();
        if (idx < node->keys.size()
            && node->keys[idx] == key) {
            if (node->children[idx]->keys.size() >= t) {
                Node* predNode = node->children[idx];
                while (!predNode->isLeaf) {
                    predNode = predNode->children.back();
                }
                T pred = predNode->keys.back();
                node->keys[idx] = pred;
                remove(node->children[idx], pred);
            }
            else if (node->children[idx + 1]->keys.size()
                >= t) {
                Node* succNode = node->children[idx + 1];
                while (!succNode->isLeaf) {
                    succNode = succNode->children.front();
                }
                T succ = succNode->keys.front();
                node->keys[idx] = succ;
                remove(node->children[idx + 1], succ);
            }
            else {
                merge(node, idx);
                remove(node->children[idx], key);
            }
        }
        else {
            if (node->children[idx]->keys.size() < t) {
                if (idx > 0
                    && node->children[idx - 1]->keys.size()
                    >= t) {
                    borrowFromPrev(node, idx);
                }
                else if (idx < node->children.size() - 1
                    && node->children[idx + 1]
                    ->keys.size()
                    >= t) {
                    borrowFromNext(node, idx);
                }
                else {
                    if (idx < node->children.size() - 1) {
                        merge(node, idx);
                    }
                    else {
                        merge(node, idx - 1);
                    }
                }
            }
            remove(node->children[idx], key);
        }
    }
}

// Implementation of borrowFromPrev function
template <typename T>
void BPlusTree<T>::borrowFromPrev(Node* node, int index)
{
    Node* child = node->children[index];
    Node* sibling = node->children[index - 1];

    child->keys.insert(child->keys.begin(),
        node->keys[index - 1]);
    node->keys[index - 1] = sibling->keys.back();
    sibling->keys.pop_back();

    if (!child->isLeaf) {
        child->children.insert(child->children.begin(),
            sibling->children.back());
        sibling->children.pop_back();
    }
}

// Implementation of borrowFromNext function
template <typename T>
void BPlusTree<T>::borrowFromNext(Node* node, int index)
{
    Node* child = node->children[index];
    Node* sibling = node->children[index + 1];

    child->keys.push_back(node->keys[index]);
    node->keys[index] = sibling->keys.front();
    sibling->keys.erase(sibling->keys.begin());

    if (!child->isLeaf) {
        child->children.push_back(
            sibling->children.front());
        sibling->children.erase(sibling->children.begin());
    }
}

// Implementation of merge function
template <typename T>
void BPlusTree<T>::merge(Node* node, int index)
{
    Node* child = node->children[index];
    Node* sibling = node->children[index + 1];

    child->keys.push_back(node->keys[index]);
    child->keys.insert(child->keys.end(),
        sibling->keys.begin(),
        sibling->keys.end());
    if (!child->isLeaf) {
        child->children.insert(child->children.end(),
            sibling->children.begin(),
            sibling->children.end());
    }

    node->keys.erase(node->keys.begin() + index);
    node->children.erase(node->children.begin() + index
        + 1);

    delete sibling;
}

// Implementation of printTree function
template <typename T>
void BPlusTree<T>::printTree(Node* node, int level)
{
    if (node != nullptr) {
        for (int i = 0; i < level; ++i) {
            cout << "  ";
        }
        if (node->isLeaf) {
            cout<< "IS LEAF" << " ";
        }
        for (const T& key : node->keys) {
            cout << key << " ";
        }
        cout << endl;
        for (Node* child : node->children) {
            printTree(child, level + 1);
        }
    }
}

// Implementation of printTree wrapper function
template <typename T> void BPlusTree<T>::printTree()
{
    printTree(root, 0);
}

// Implementation of search function
template <typename T> bool BPlusTree<T>::search(T key)
{
    Node* current = root;
    while (current != nullptr) {
        int i = 0;
        while (i < current->keys.size()
            && key > current->keys[i]) {
            i++;
        }
        if (i < current->keys.size()
            && key == current->keys[i]) {
            return true;
        }
        if (current->isLeaf) {
            return false;
        }
        current = current->children[i];
    }
    return false;
}

// Implementation of range query function
template <typename T>
vector<T> BPlusTree<T>::rangeQuery(T lower, T upper)
{
    vector<T> result;
    Node* current = root;
    while (!current->isLeaf) {
        int i = 0;
        while (i < current->keys.size()
            && lower > current->keys[i]) {
            i++;
        }
        current = current->children[i];
    }
    while (current != nullptr) {
        for (const T& key : current->keys) {
            if (key >= lower && key <= upper) {
                result.push_back(key);
            }
            if (key > upper) {
                return result;
            }
        }
        current = current->next;
    }
    return result;
}

// Implementation of insert function
template <typename T> void BPlusTree<T>::insert(T key)
{
    numOfElements++;
    if (root == nullptr) {
        root = new Node(true);
        root->keys.push_back(key);
    }
    else {
        if (root->keys.size() == 2 * t - 1) {
            Node* newRoot = new Node();
            newRoot->children.push_back(root);
            splitChild(newRoot, 0, root);
            root = newRoot;
        }
        insertNonFull(root, key);
    }
}

// Implementation of remove function
template <typename T> void BPlusTree<T>::remove(T key)
{
    if (root == nullptr) {
        return;
    }
    remove(root, key);
    if (root->keys.empty() && !root->isLeaf) {
        Node* tmp = root;
        root = root->children[0];
        delete tmp;
    }
}



template <typename T> void BPlusTree<T>::writeToFile(Node* node, fstream& dbFile,int page) {
    if (!node) {
        return;
    }
    firstFreePage++;

    streampos pos = streampos(page * PAGE_SIZE);
    dbFile.seekp(pos);

    if (node->isLeaf)
    {
        dbFile.write(reinterpret_cast<char*>(&isLeafValue), sizeof(isLeafValue));
        dbFile.write(reinterpret_cast<char*>(&isLeafValue), sizeof(isLeafValue));//dummy value will get overwritten
        childrenLocation->push_back(page);
    }
    else dbFile.write(reinterpret_cast<char*>(&isNotLeafValue), sizeof(isNotLeafValue));

    int nodeKeysSize = node->keys.size();


    dbFile.write(reinterpret_cast<const char*>(&nodeKeysSize), sizeof(nodeKeysSize));

    // Write keys
    for (const auto& key : node->keys) {
        dbFile.write(reinterpret_cast<const char*>(&key), sizeof(T));
    }

    int childrenSize = node->children.size();
    dbFile.write(reinterpret_cast<char*>(&childrenSize), sizeof(childrenSize));

    //Children
    for (size_t i = 0; i < node->children.size(); i++) {
        dbFile.write(reinterpret_cast<const char*>(&firstFreePage), sizeof(firstFreePage));
        pos = dbFile.tellp();
        writeToFile(node->children[i], dbFile, firstFreePage);
        dbFile.seekp(pos);
    }

}



template <typename T> bool BPlusTree<T>::writeToFile(string filename,int page_size)
{
    fstream dbFile(filename,std::ios::binary | ios::in | ios::out);
    if (!dbFile) {
        std::cerr << "Error: could not open file!\n";
        return false;
    }

    Node* current = root;
    firstFreePage = 0;
    childrenLocation->clear();
    writeToFile(root,dbFile,firstFreePage);

    for (int i = 0; i < childrenLocation->size(); i++) {
        int page = childrenLocation->at(i);
        int nextPage = i != childrenLocation->size() - 1 ? childrenLocation->at(i+1) : -1;
        cout << page << " ";
        streampos pos = streampos(page * PAGE_SIZE+ sizeof(int));
        dbFile.seekp(pos);
        dbFile.write(reinterpret_cast<char*>(&nextPage), sizeof(nextPage));
    }
    dbFile.close();

    return true;
}



template <typename T> typename BPlusTree<T>::PageNode* BPlusTree<T>::pageToNode(fstream& dbFile, int page){

    if (page < 0) {
        return nullptr;
    }
    PageNode* res = new PageNode();
    streampos pos = streampos(page * PAGE_SIZE);
    dbFile.seekp(pos);
    int val = 0;
    dbFile.read(reinterpret_cast<char*>(&val), sizeof(val));
    if (val == isLeafValue) {
        res->isLeaf = true;
        dbFile.read(reinterpret_cast<char*>(&val), sizeof(val));
        res->nextLeafPage = val;
    }
    dbFile.read(reinterpret_cast<char*>(&val), sizeof(val));
    int numberOfKeys = val;
    for (int i = 0; i < numberOfKeys; i++) {
        dbFile.read(reinterpret_cast<char*>(&val), sizeof(val));
        res->keys.push_back(val);
    }

    if (res->isLeaf) return res;

    dbFile.read(reinterpret_cast<char*>(&val), sizeof(val));
    int numberOfChildren = val;
    for (int i = 0; i < numberOfKeys; i++) {
        dbFile.read(reinterpret_cast<char*>(&val), sizeof(val));
        res->childrenPages.push_back(val);
    }

    return res;
}
 

template <typename T> vector<T>* BPlusTree<T>::getFromFile(string Filename) {
    fstream dbFile(Filename, ios_base::binary | ios_base::in | ios_base::out);
    int currentPage = 0;

    vector<T>* res = new vector<T>();

    PageNode* current = pageToNode(dbFile, currentPage);

    while (!current->isLeaf) {
        current = pageToNode(dbFile, current->childrenPages[0]);
    }
    
    while (current) {
        res->insert(res->end(), current->keys.begin(), current->keys.end());
        current = pageToNode(dbFile, current->nextLeafPage);
    }
    
    return res;

}




template<typename T> typename BPlusTree<T>::Node* BPlusTree<T>::bottom_up(const vector<T>& sorted_keys) {
    if (sorted_keys.empty()) {
        return nullptr;
    }

    // Create leaf nodes
    vector<Node*> leaves;
    int n = sorted_keys.size();
    int leaf_capacity = 2*t - 1;
    int num_leaves = ceil(static_cast<double>(n) / leaf_capacity);

    // Create and populate leaf nodes
    for (int i = 0; i < num_leaves; i++) {
        Node* leaf = new Node(true);
        int start = i * leaf_capacity;
        int end = min(start + leaf_capacity, n);

        for (int j = start; j < end; j++) {
            leaf->keys.push_back(sorted_keys[j]);
        }
        leaves.push_back(leaf);
    }

    // Link leaf nodes
    for (int i = 0; i < leaves.size() - 1; i++) {
        leaves[i]->next = leaves[i + 1];
    }

    // Build the tree from bottom up
    vector<Node*> current_level = leaves;

    while (current_level.size() > 1) {
        vector<Node*> next_level;
        int internal_capacity = 2*t-1;
        int num_nodes = ceil(static_cast<double>(current_level.size()) / internal_capacity);
        cout << "NUM NODES:" << num_nodes << "\n";

        for (int i = 0; i < num_nodes; i++) {
            Node* internal_node = new Node(false);
            int start = i * internal_capacity;
            int end = min(start + internal_capacity, (int)current_level.size());

            // Add first child
            internal_node->children.push_back(current_level[start]);

            // Add keys and remaining children
            for (int j = start + 1; j < end; j++) {
                // The key is the first key of the child node
                internal_node->keys.push_back(current_level[j]->keys[0]);
                internal_node->children.push_back(current_level[j]);
            }

            next_level.push_back(internal_node);
        }

        current_level = next_level;
    }

    root = current_level[0];

    return current_level[0]; // Return root
}








#endif // !BTREE
