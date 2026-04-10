#include "../h/SkipList.hpp"

#include <climits>
#include <iostream>
#include <random>

SkipList::SkipList() : head(new SkipListNode(INT_MIN, 0)), level(0), size(0) {}

SkipList::~SkipList() {
    SkipListNode* node = head->next[0];
    while (node != nullptr) {
        SkipListNode* nxt = node->next[0];
        delete node;
        node = nxt;
    }
    delete head;
}

int SkipList::random_level() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::bernoulli_distribution coin(0.5);
    int lvl = 0;
    while (coin(gen) && lvl < MAX_LEVEL - 1) {
        ++lvl;
    }
    return lvl;
}

int SkipList::search(int key) {
    SkipListNode* x = head;
    for (int i = level; i >= 0; i--) {
        while (x->next[i] != nullptr && x->next[i]->key < key) {
            x = x->next[i];
        }
    }
    x = x->next[0];
    if (x != nullptr && x->key == key) {
        return x->value;
    }
    return -1;
}

void SkipList::insert(int key, int value) {
    SkipListNode* update[MAX_LEVEL];
    SkipListNode* x = head;
    for (int i = level; i >= 0; i--) {
        while (x->next[i] != nullptr && x->next[i]->key < key) {
            x = x->next[i];
        }
        update[i] = x;
    }
    x = x->next[0];
    if (x != nullptr && x->key == key) {
        x->value = value;
        return;
    }

    int newLevel = random_level();
    if (newLevel > level) {
        for (int i = level + 1; i <= newLevel; i++) {
            update[i] = head;
        }
        level = newLevel;
    }

    SkipListNode* newNode = new SkipListNode(key, value);
    for (int i = 0; i <= newLevel; i++) {
        newNode->next[i] = update[i]->next[i];
        update[i]->next[i] = newNode;
    }
    size++;
}

void SkipList::remove(int key) {
    SkipListNode* update[MAX_LEVEL];
    SkipListNode* x = head;
    for (int i = level; i >= 0; i--) {
        while (x->next[i] != nullptr && x->next[i]->key < key) {
            x = x->next[i];
        }
        update[i] = x;
    }
    x = x->next[0];
    if (x == nullptr || x->key != key) {
        return;
    }
    for (int i = 0; i <= level; i++) {
        if (update[i]->next[i] != x) {
            break;
        }
        update[i]->next[i] = x->next[i];
    }
    delete x;
    size--;
    while (level > 0 && head->next[level] == nullptr) {
        level--;
    }
}

void SkipList::print() {
    SkipListNode* x = head->next[0];
    while (x != nullptr) {
        std::cout << "(" << x->key << ", " << x->value << ") ";
        x = x->next[0];
    }
    std::cout << "\n";
}
