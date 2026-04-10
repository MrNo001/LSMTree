#include "../h/SkipList.hpp"

#include <cassert>
#include <iostream>
#include <vector>

int main() {
    SkipList list;

    std::vector<std::pair<int, int>> values = {
        {10, 100},
        {3, 30},
        {7, 70},
        {1, 10},
        {20, 200}
    };

    for (const auto& [key, value] : values) {
        list.insert(key, value);
    }

    std::cout << "SkipList contents (key, value): ";
    list.print();

    // Basic sanity checks.
    assert(list.search(7) == 70);
    assert(list.search(20) == 200);
    assert(list.search(999) == -1);

    list.remove(7);
    std::cout << "After removing key 7: ";
    list.print();
    assert(list.search(7) == -1);

    std::cout << "SkipList test passed.\n";
    return 0;
}
