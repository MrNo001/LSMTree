#ifndef SKIPLIST_HPP
#define SKIPLIST_HPP

#include <utility>
#include <vector>

#define MAX_LEVEL 10

class SkipListNode {
    public:
    int key;
    int value;
    SkipListNode* next[MAX_LEVEL];
    SkipListNode(int key, int value) : key(key), value(value) {
        for (int i = 0; i < MAX_LEVEL; i++) {
            next[i] = nullptr;
        }
    }
};

class SkipList {
    public:
    SkipList();
    ~SkipList();
    void insert(int key, int value);
    void remove(int key);
    int search(int key);
    void print();
    int random_level();
    int getSize() const;
    void clear();
    std::vector<std::pair<int, int>> sortedEntries() const;

    private:
    SkipListNode* head;
    int level;
    int size;
};

#endif // SKIPLIST_HPP
