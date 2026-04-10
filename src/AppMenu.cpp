	#include "../h/AppMenu.hpp"
	
	#include <iostream>
	#include <string>
	#include "BTree.cpp"
	#include "../h/LogBTree.hpp"
	
	using namespace std;
	
	BPlusTree<int>* buildIndexFromCSV(const std::string& filename);
	void searchAndDisplayRecord(BPlusTree<int>* tree, const std::string& filename, int searchId);
	void displayAllRecords(BPlusTree<int>* tree, const std::string& filename);
	void testRemoveFunction();
	void testLogBTree();
	void cleanupLogBTreeFiles(const std::string& baseName, int levels);
	
	void AppMenu::printMenu() {
		cout << "\n=== B+ Tree Database Index Manager ===" << endl;
		cout << "1. Run Test1" << endl;
		cout << "2. Generate Tree from CSV File" << endl;
		cout << "3. Insert Key-Value Pair" << endl;
		cout << "4. Remove Key" << endl;
		cout << "5. Search for Key" << endl;
		cout << "6. Range Query" << endl;
		cout << "7. Print Tree Structure" << endl;
		cout << "8. Save Tree to File" << endl;
		cout << "9. Load Tree from File" << endl;
		cout << "10. Display All Records" << endl;
		cout << "11. Test Remove Function (Random Removals)" << endl;
		cout << "12. Test LSM Tree Implementation" << endl;
		cout << "13. LSM Tree Operations" << endl;
		cout << "0. Exit" << endl;
		cout << "Enter your choice: ";
	}
	
	void AppMenu::logBTreeOperations() {
		cout << "=== Log B+ Tree Operations ===" << endl;
		
		string filename;
		cout << "Enter CSV filename to index: ";
		cin.ignore();
		getline(cin, filename);
		
		if (filename.empty()) {
			cout << "No filename provided. Using default: database_files/sample_database_200.csv" << endl;
			filename = "database_files/sample_database_200.csv";
		}
		
		vector<int> levelCapacities = {80, 200, 500};
		const int levels = static_cast<int>(levelCapacities.size());
		const string logBTreeName = "Interactive";
		
		cleanupLogBTreeFiles(logBTreeName, levels);
		
		LogBTree* logBTree = new LogBTree(3, levels, levelCapacities, logBTreeName);
		
		while (true) {
			cout << "\n=== Log B+ Tree Operations Menu ===" << endl;
			cout << "1. Insert Key-Value Pair" << endl;
			cout << "2. Search for Key" << endl;
			cout << "3. Print All Records" << endl;
			cout << "4. Print Log B+ Tree Statistics" << endl;
			cout << "5. Insert Multiple Keys (1-100)" << endl;
			cout << "6. Insert Multiple Keys (101-200)" << endl;
			cout << "7. Build Index from CSV File" << endl;
			cout << "0. Back to Main Menu" << endl;
			cout << "Enter your choice: ";
			
			int choice;
			cin >> choice;
			
			switch (choice) {
				case 1: {
					int key, value;
					cout << "Enter key to insert: ";
					cin >> key;
					cout << "Enter value: ";
					cin >> value;
					logBTree->insert(key, value);
					cout << "Inserted key " << key << " with value " << value << endl;
					cout << "Memory elements: " << logBTree->getMemoryElements() 
						 << ", Disk files: " << logBTree->getDiskFiles() 
						 << ", Total: " << logBTree->getTotalElements() << endl;
					break;
				}
				case 2: {
					int key;
					cout << "Enter key to search: ";
					cin >> key;
					int* result = logBTree->search(key);
					if (result) {
						cout << "Found key " << key << " at file position: " << *result << endl;
						
						ifstream file(filename);
						if (file.is_open()) {
							file.seekg(*result);
							string line;
							if (getline(file, line)) {
								cout << "Key: " << key << " | File Position: " << *result << " | Record: " << line << endl;
							} else {
								cout << "Could not read record from file at position " << *result << endl;
							}
							file.close();
						} else {
							cout << "Could not open file " << filename << endl;
						}
					} else {
						cout << "Key " << key << " not found!" << endl;
					}
					break;
				}
				case 3: {
					cout << "\n=== All Records in Log B+ Tree ===" << endl;
					auto allRecords = logBTree->getAllRecords();
					cout << "Total records: " << allRecords.size() << endl;
					
					int showCount = static_cast<int>(allRecords.size());
					cout << "\nAll " << showCount << " records:" << endl;
					
					ifstream file(filename);
					if (!file.is_open()) {
						cerr << "Error: Could not open file " << filename << endl;
						break;
					}
					
					for (int i = 0; i < showCount; i++) {
						int key = allRecords[i].first;
						int position = allRecords[i].second;
						
						file.seekg(position);
						string line;
						if (getline(file, line)) {
							cout << "Key: " << key << " | File Position: " << position << " | Record: " << line << endl;
						} else {
							cout << "Key: " << key << " | File Position: " << position << " | Record: [Could not read from file]" << endl;
						}
					}
					
					file.close();
					break;
				}
				case 4: {
					cout << "\n=== Log B+ Tree Statistics ===" << endl;
					cout << "Memory elements: " << logBTree->getMemoryElements() << endl;
					cout << "Disk files: " << logBTree->getDiskFiles() << endl;
					cout << "Total elements: " << logBTree->getTotalElements() << endl;
					cout << "Memory limit: " << levelCapacities[0] << " keys" << endl;
					
					if (logBTree->getDiskFiles() > 0) {
						cout << "Disk files pattern: Log_" << logBTreeName << "_LVL#.bin" << endl;
					} else {
						cout << "No disk files (all data in memory)" << endl;
					}
					break;
				}
				case 5: {
					cout << "Inserting keys 1-100..." << endl;
					for (int i = 1; i <= 100; i++) {
						logBTree->insert(i, i * 100);
						if (i % 20 == 0) {
							cout << "Inserted " << i << " keys. Memory: " << logBTree->getMemoryElements() 
								 << ", Disk files: " << logBTree->getDiskFiles() << endl;
						}
					}
					cout << "Completed inserting keys 1-100!" << endl;
					break;
				}
				case 6: {
					cout << "Inserting keys 101-200..." << endl;
					for (int i = 101; i <= 200; i++) {
						logBTree->insert(i, i * 100);
						if (i % 20 == 0) {
							cout << "Inserted " << i << " keys. Memory: " << logBTree->getMemoryElements() 
								 << ", Disk files: " << logBTree->getDiskFiles() << endl;
						}
					}
					cout << "Completed inserting keys 101-200!" << endl;
					break;
				}
				case 7: {
					cout << "\n=== Building Log B+ Tree Index from CSV File ===" << endl;
					cout << "Using file: " << filename << endl;
					
					ifstream file(filename);
					if (!file.is_open()) {
						cerr << "Error: Could not open file " << filename << endl;
						break;
					}
					
					string line;
					streampos position;
					
					getline(file, line);
					cout << "Header: " << line << endl;
					
					int count = 0;
					while (getline(file, line)) {
						if (line.empty()) continue;
						
						position = file.tellg() - static_cast<streampos>(line.length()+2);
						
						int id = stoi(line.substr(0, line.find(',')));
						logBTree->insert(id, static_cast<int>(position));
						count++;
						
						if (count % 50 == 0) {
							cout << "Inserted " << count << " records. Memory: " << logBTree->getMemoryElements() 
								 << ", Disk files: " << logBTree->getDiskFiles() 
								 << ", Total: " << logBTree->getTotalElements() << endl;
						}
					}
					
					file.close();
					cout << "Built Log B+ Tree index with " << count << " records from " << filename << endl;
					cout << "Final state - Memory: " << logBTree->getMemoryElements() 
						 << ", Disk files: " << logBTree->getDiskFiles() 
						 << ", Total: " << logBTree->getTotalElements() << endl;
					break;
				}
				case 0: {
					delete logBTree;
					return;
				}
				default: {
					cout << "Invalid choice. Please try again." << endl;
					break;
				}
			}
			
			cout << "\nPress Enter to continue...";
			cin.ignore();
			cin.get();
		}
	}
	
	void AppMenu::run() {
		BPlusTree<int>* currentTree = nullptr;
		string currentFilename = "";
		
		while (true) {
			printMenu();
			
			int choice;
			cin >> choice;
			
			switch (choice) {
				case 1: {
					extern void test1();
					test1();
					break;
				}
				case 2: {
					cout << "Enter CSV filename: ";
					cin >> currentFilename;
					if (currentTree) {
						delete currentTree;
					}
					currentTree = buildIndexFromCSV(currentFilename);
					if (currentTree) {
						cout << "Tree generated successfully with " << currentTree->numOfElements << " elements." << endl;
					} else {
						cout << "Failed to generate tree from file." << endl;
					}
					break;
				}
				case 3: {
					if (!currentTree) {
						cout << "No tree loaded. Please generate a tree first (option 2)." << endl;
						break;
					}
					int key, value;
					cout << "Enter key to insert: ";
					cin >> key;
					cout << "Enter value (position): ";
					cin >> value;
					currentTree->insert(key, value);
					cout << "Inserted key " << key << " with value " << value << endl;
					cout << "Total elements: " << currentTree->numOfElements << endl;
					break;
				}
				case 4: {
					if (!currentTree) {
						cout << "No tree loaded. Please generate a tree first (option 2)." << endl;
						break;
					}
					int key;
					cout << "Enter key to remove: ";
					cin >> key;
					int* pos = currentTree->search(key);
					if (pos) {
						currentTree->remove(key);
						cout << "Removed key " << key << endl;
						cout << "Total elements: " << currentTree->numOfElements << endl;
					} else {
						cout << "Key " << key << " not found in tree." << endl;
					}
					break;
				}
				case 5: {
					if (!currentTree) {
						cout << "No tree loaded. Please generate a tree first (option 2)." << endl;
						break;
					}
					int key;
					cout << "Enter key to search: ";
					cin >> key;
					int* pos = currentTree->search(key);
					if (pos) {
						cout << "Key " << key << " found with value " << *pos << endl;
						if (!currentFilename.empty()) {
							searchAndDisplayRecord(currentTree, currentFilename, key);
						}
					} else {
						cout << "Key " << key << " not found." << endl;
					}
					break;
				}
				case 6: {
					if (!currentTree) {
						cout << "No tree loaded. Please generate a tree first (option 2)." << endl;
						break;
					}
					int lower, upper;
					cout << "Enter lower bound: ";
					cin >> lower;
					cout << "Enter upper bound: ";
					cin >> upper;
					auto results = currentTree->rangeQuery(lower, upper);
					cout << "Range query [" << lower << ", " << upper << "] found " << results.size() << " results:" << endl;
					for (const auto& record : results) {
						cout << "  Key: " << record.first << ", Value: " << record.second << endl;
					}
					break;
				}
				case 7: {
					if (!currentTree) {
						cout << "No tree loaded. Please generate a tree first (option 2)." << endl;
						break;
					}
					cout << "Tree Structure:" << endl;
					currentTree->printTree();
					break;
				}
				case 8: {
					if (!currentTree) {
						cout << "No tree loaded. Please generate a tree first (option 2)." << endl;
						break;
					}
					string filename;
					cout << "Enter filename to save tree: ";
					cin.ignore();
					getline(cin, filename);
					
					if (filename.empty()) {
						cout << "No filename provided. Please try again." << endl;
						break;
					}
					
					if (currentTree->writeToFile(filename)) {
						cout << "Tree saved to " << filename << endl;
					} else {
						cout << "Failed to save tree to " << filename << endl;
					}
					break;
				}
				case 9: {
					string filename;
					cout << "Enter filename to load tree: ";
					cin.ignore();
					getline(cin, filename);
					
					if (filename.empty()) {
						cout << "No filename provided. Please try again." << endl;
						break;
					}
					
					if (currentTree) {
						delete currentTree;
					}
					auto records = BPlusTree<int>(3).getFromFile(filename);
					if (records && !records->empty()) {
						currentTree = new BPlusTree<int>(3);
						currentTree->bottom_up(*records);
						cout << "Tree loaded from " << filename << " with " << currentTree->numOfElements << " elements." << endl;
						delete records;
					} else {
						cout << "Failed to load tree from " << filename << endl;
					}
					break;
				}
				case 10: {
					if (!currentTree) {
						cout << "No tree loaded. Please generate a tree first (option 2)." << endl;
						break;
					}
					if (currentFilename.empty()) {
						cout << "No CSV file associated with current tree." << endl;
						break;
					}
					displayAllRecords(currentTree, currentFilename);
					break;
				}
				case 11: {
					testRemoveFunction();
					break;
				}
				case 12: {
					testLogBTree();
					break;
				}
				case 13: {
					logBTreeOperations();
					break;
				}
				case 0: {
					if (currentTree) {
						delete currentTree;
					}
					cout << "Goodbye!" << endl;
					return;
				}
				default: {
					cout << "Invalid choice. Please try again." << endl;
					break;
				}
			}
			
			cout << "\nPress Enter to continue...";
			cin.ignore();
			cin.get();
		}
	}
	

