#include <cstdio>
#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>
#include <random>
#include <algorithm>
#include "BTree.cpp"
#include "../h/LogBTree.hpp"
#include "../h/AppMenu.hpp"

using namespace std;

void cleanupLogBTreeFiles(const string& baseName, int levels) {
    for (int level = 1; level < levels; ++level) {
        string filename = "Log_" + baseName + "_LVL" + to_string(level) + ".bin";
        remove(filename.c_str());
    }
}



int extractId(const string& line) {
    stringstream ss(line);
    string idStr;
    getline(ss, idStr, ',');
    return stoi(idStr);
}

BPlusTree<int>* buildIndexFromCSV(const string& filename) {
    ifstream file(filename);
    if (!file.is_open()) {
        cerr << "Error: Could not open file " << filename << endl;
        return nullptr;
    }

    BPlusTree<int>* tree = new BPlusTree<int>(3);

    string line;
    streampos position;

    getline(file, line);
    cout << "Header: " << line << endl;

    while (getline(file, line)) {
        if (line.empty()) continue;  

        // Get position before reading the line
        position = file.tellg() - static_cast<streampos>(line.length()+2); // +1 for newline

        int id = extractId(line);

        tree->insert(id, static_cast<int>(position));

        cout << "Inserted ID: " << id << " at position: " << position << endl;
    }

    file.close();
    return tree;
}

void searchAndDisplayRecord(BPlusTree<int>* tree, const string& filename, int searchId) {
    if (!tree) {
        cout << "Tree is null!" << endl;
        return;
    }

    int* positionPtr = tree->search(searchId);

    if (positionPtr == nullptr) {
        cout << "ID " << searchId << " not found in the index." << endl;
        return;
    }

    int position = *positionPtr;
    cout << "Found ID " << searchId << " at file position: " << position << endl;

    ifstream file(filename);
    if (!file.is_open()) {
        cerr << "Error: Could not open file " << filename << endl;
        return;
    }

    file.seekg(position);
    string record;
    if (getline(file, record)) {
        cout << "Key: " << searchId << " | File Position: " << position << " | Record: " << record << endl;

        stringstream ss(record);
        string id, name, country, gender;

        getline(ss, id, ',');
        getline(ss, name, ',');
        getline(ss, country, ',');
        getline(ss, gender, ',');

        cout << "Parsed fields:" << endl;
        cout << "  ID: " << id << endl;
        cout << "  Name: " << name << endl;
        cout << "  Country: " << country << endl;
        cout << "  Gender: " << gender << endl;
    }
    else {
        cout << "Error: Could not read record at position " << position << endl;
    }

    file.close();
}

void rangeQueryDemo(BPlusTree<int>* tree, const string& filename, int lower, int upper) {
    if (!tree) {
        cout << "Tree is null!" << endl;
        return;
    }

    cout << "Range query for IDs between " << lower << " and " << upper << ":" << endl;

    auto results = tree->rangeQuery(lower, upper);

    ifstream file(filename);
    if (!file.is_open()) {
        cerr << "Error: Could not open file " << filename << endl;
        return;
    }

    for (const auto& record : results) {
        int id = record.first;
        int position = record.second;

        file.seekg(position);
        string line;
        if (getline(file, line)) {
            cout << "Key: " << id << " | File Position: " << position << " | Record: " << line << endl;
        }
    }

    file.close();
}

void displayAllRecords(BPlusTree<int>* tree, const string& filename) {
    if (!tree) {
        cout << "Tree is null!" << endl;
        return;
    }

    cout << "All records in the database:" << endl;

    // Get all records by traversing leaf nodes
    auto results = tree->getAllRecords(); 

    ifstream file(filename);
    if (!file.is_open()) {
        cerr << "Error: Could not open file " << filename << endl;
        return;
    }

    for (const auto& record : results) {
        int id = record.first;
        int position = record.second;

        file.seekg(position);
        string line;
        if (getline(file, line)) {
            cout << "Key: " << id << " | File Position: " << position << " | Record: " << line << endl;
        }
    }

    file.close();
}

void testRemoveFunction() {
    const string filename = "database_files/sample_database_200.csv";  

    cout << "=== Testing Remove Function with Random Removals ===" << endl;
    BPlusTree<int>* tree = buildIndexFromCSV(filename);

    if (!tree) {
        cerr << "Failed to build index!" << endl;
        return;
    }

    cout << "\n=== Initial Tree State ===" << endl;
    cout << "Number of elements: " << tree->numOfElements << endl;

    // Get all keys for random selection
    auto allRecords = tree->getAllRecords();
    vector<int> allKeys;
    for (const auto& record : allRecords) {
        allKeys.push_back(record.first);
    }

    // Randomly shuffle the keys
    random_device rd;
    mt19937 g(rd());
    shuffle(allKeys.begin(), allKeys.end(), g);

    // Test just one removal for debugging
    cout << "\n=== Testing Single Removal for Debug ===" << endl;
    
    int keyToRemove = allKeys[0]; // Remove the first key
    cout << "Removing key " << keyToRemove << "..." << endl;
    
    // Verify key exists before removal
    int* pos = tree->search(keyToRemove);
    if (!pos) {
        cout << "ERROR: Key " << keyToRemove << " not found before removal!" << endl;
        delete tree;
        return;
    }
    cout << "Key " << keyToRemove << " found before removal with value: " << *pos << endl;
    
    // Remove the key
    cout << "\n--- Starting removal process ---" << endl;
    tree->remove(keyToRemove);
    cout << "--- Removal process completed ---" << endl;
    
    // Debug: Check if the key is still in the tree by traversing all records
    auto allRecordsAfter = tree->getAllRecords();
    bool keyStillExists = false;
    for (const auto& record : allRecordsAfter) {
        if (record.first == keyToRemove) {
            keyStillExists = true;
            break;
        }
    }
    
    // Verify key is gone after removal
    pos = tree->search(keyToRemove);
    if (pos || keyStillExists) {
        cout << "ERROR: Key " << keyToRemove << " still exists after removal!" << endl;
        cout << "Search result: " << (pos ? "found" : "not found") << endl;
        cout << "Traversal result: " << (keyStillExists ? "found" : "not found") << endl;
        if (pos) {
            cout << "Search returned value: " << *pos << endl;
        }
        delete tree;
        return;
    }
    
    cout << "SUCCESS: Key " << keyToRemove << " was properly removed!" << endl;

    cout << "\n=== Final Tree State ===" << endl;
    cout << "Number of elements after removals: " << tree->numOfElements << endl;
    cout << "Expected elements: " << (allKeys.size() - 1) << endl;
    
    if (tree->numOfElements == (allKeys.size() - 1)) {
        cout << "Element count is correct!" << endl;
    } else {
        cout << "Element count mismatch!" << endl;
    }

    // Verify remaining keys are still accessible
    cout << "\n=== Verifying Remaining Keys ===" << endl;
    vector<int> remainingKeys(allKeys.begin() + 1, allKeys.end());
    bool allRemainingFound = true;
    
    for (int key : remainingKeys) {
        int* pos = tree->search(key);
        if (!pos) {
            cout << "ERROR: Remaining key " << key << " not found!" << endl;
            allRemainingFound = false;
        }
    }
    
    if (allRemainingFound) {
        cout << "All remaining keys are accessible!" << endl;
    } else {
        cout << "Some remaining keys are missing!" << endl;
    }

    // Test tree structure integrity
    cout << "\n=== Testing Tree Structure Integrity ===" << endl;
    auto finalRecords = tree->getAllRecords();
    cout << "Records retrieved by traversal: " << finalRecords.size() << endl;
    
    if (finalRecords.size() == tree->numOfElements) {
        cout << "Tree structure is consistent!" << endl;
    } else {
        cout << "Tree structure inconsistency detected!" << endl;
    }

    cout << "\n=== Remove Function Test " << (allRemainingFound && finalRecords.size() == tree->numOfElements ? "PASSED" : "FAILED") << " ===" << endl;

    delete tree;
}

void testLogBTree() {
    cout << "=== Testing Log B+ Tree Implementation ===" << endl;
    
    vector<int> levelCapacities = {80, 200, 500};
    const int levels = static_cast<int>(levelCapacities.size());
    const string logBTreeName = "Test";
    cleanupLogBTreeFiles(logBTreeName, levels);
    
    LogBTree* logBTree = new LogBTree(3, levels, levelCapacities, logBTreeName);
    
    cout << "Created Log B+ Tree with memory limit of " << levelCapacities[0] << " keys" << endl;
    
    // Insert 200 keys to trigger multiple merges
    cout << "\n=== Phase 1: Inserting 200 keys ===" << endl;
    for (int i = 1; i <= 200; i++) {
        logBTree->insert(i, i * 100);
        
        // Show progress every 20 insertions
        if (i % 20 == 0) {
                    cout << "Inserted " << i << " keys. Memory: " << logBTree->getMemoryElements() 
                 << ", Disk files: " << logBTree->getDiskFiles() 
                 << ", Total: " << logBTree->getTotalElements() << endl;
        }
    }
    
    cout << "\n=== Phase 2: Testing Search Functionality ===" << endl;
    
    // Test searches in memory
    cout << "Testing searches in memory tree..." << endl;
    for (int i = 190; i <= 200; i++) {
        int* result = logBTree->search(i);
        if (result && *result == i * 100) {
            cout << "Key " << i << " found in memory: " << *result << endl;
        } else {
            cout << "Key " << i << " not found in memory!" << endl;
        }
    }
    
    // Test searches in disk files
    cout << "\nTesting searches in disk files..." << endl;
    for (int i = 1; i <= 10; i++) {
        int* result = logBTree->search(i);
        if (result && *result == i * 100) {
            cout << "Key " << i << " found in disk: " << *result << endl;
        } else {
            cout << "Key " << i << " not found in disk!" << endl;
        }
    }
    
    cout << "\n=== Phase 3: Testing GetAllRecords ===" << endl;
    auto allRecords = logBTree->getAllRecords();
    cout << "Total records retrieved: " << allRecords.size() << endl;
    
    // Verify all records are present and sorted
    bool allPresent = true;
    bool isSorted = true;
    
    for (int i = 1; i <= 200; i++) {
        bool found = false;
        for (const auto& record : allRecords) {
            if (record.first == i) {
                found = true;
                if (record.second != i * 100) {
                    cout << "Wrong value for key " << i << ": expected " << (i * 100) << ", got " << record.second << endl;
                    allPresent = false;
                }
                break;
            }
        }
        if (!found) {
            cout << "Key " << i << " missing from records!" << endl;
            allPresent = false;
        }
    }
    
    // Check if sorted
    for (size_t i = 1; i < allRecords.size(); i++) {
        if (allRecords[i-1].first >= allRecords[i].first) {
            cout << "Records not sorted: " << allRecords[i-1].first << " >= " << allRecords[i].first << endl;
            isSorted = false;
            break;
        }
    }
    
    if (allPresent) {
        cout << "All 200 keys present with correct values!" << endl;
    }
    
    if (isSorted) {
        cout << "All records are properly sorted!" << endl;
    }
    
    cout << "\n=== Phase 4: Final Statistics ===" << endl;
        cout << "Memory elements: " << logBTree->getMemoryElements() << endl;
    cout << "Disk files: " << logBTree->getDiskFiles() << endl;
    cout << "Total elements: " << logBTree->getTotalElements() << endl;
    
    // Test some random searches
    cout << "\n=== Phase 5: Random Search Test ===" << endl;
    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<> dis(1, 200);
    
    bool randomSearchesWork = true;
    for (int i = 0; i < 10; i++) {
        int randomKey = dis(gen);
        int* result = logBTree->search(randomKey);
        if (result && *result == randomKey * 100) {
            cout << "Random key " << randomKey << " found: " << *result << endl;
        } else {
            cout << "Random key " << randomKey << " not found!" << endl;
            randomSearchesWork = false;
        }
    }
    
    if (randomSearchesWork) {
        cout << "All random searches successful!" << endl;
    }
    
    cout << "\n=== Log B+ Tree Test Results ===" << endl;
    if (allPresent && isSorted && randomSearchesWork) {
        cout << "Log B+ Tree Test PASSED! All functionality working correctly." << endl;
    } else {
        cout << "Log B+ Tree Test FAILED! Some issues detected." << endl;
    }
    
    delete logBTree;
}

void test1() {
    const string filename = "database_files/sample_database_200.csv";  

    cout << "=== Building B+ Tree Index from CSV File ===" << endl;
    BPlusTree<int>* tree_test = buildIndexFromCSV(filename);

    if (!tree_test) {
        cerr << "Failed to build index!" << endl;
        return;
    }

    cout << "\n=== Index Built Successfully ===" << endl;
    cout << "Number of elements in tree: " << tree_test->numOfElements << endl;

    // Demo: Print the tree structure
    cout << "\n=== Tree Structure ===" << endl;
    tree_test->printTree();

    // Demo: Search for specific records
    cout << "\n=== Searching for Specific Records ===" << endl;
    searchAndDisplayRecord(tree_test, filename, 5);  
    searchAndDisplayRecord(tree_test, filename, 8);  
    searchAndDisplayRecord(tree_test, filename, 15);

    cout << "\n=== Range Query Demo ===" << endl;
    rangeQueryDemo(tree_test, filename, 3, 7);  

    cout << "\n=== Displaying All Records ===" << endl;
    displayAllRecords(tree_test, filename);

    cout << "\n=== Testing Remove Function ===" << endl;
    cout << "Before removal - searching for ID 5: ";
    int* pos = tree_test->search(5);
    cout << (pos ? "Found" : "Not found") << endl;
    
    cout << "Removing ID 5..." << endl;
    tree_test->remove(5);
    
    cout << "After removal - searching for ID 5: ";
    pos = tree_test->search(5);
    cout << (pos ? "Found" : "Not found") << endl;
    
    cout << "Number of elements after removal: " << tree_test->numOfElements << endl;

    cout << "\n=== Saving Index to File ===" << endl;
    if (tree_test->writeToFile("index_test1.bin")) {
        cout << "Index saved to index_test1.bin" << endl;
    }
    else {
        cout << "Failed to save index" << endl;
    }

    tree_test->getFromFile("index_test1.bin");
    tree_test->printTree();

    // Clean up
    delete tree_test;

    cout << "\n=== Test1 Completed ===" << endl;
}

int main() {
	AppMenu menu;
	menu.run();
	return 0;
}