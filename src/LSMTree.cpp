	#include "../h/LSMTree.hpp"
	#include <algorithm>
	#include <filesystem>
	#include <iostream>
	#include <map>
	#include <stdexcept>
	
	using namespace std;
	namespace fs = std::filesystem;
	
	LSMTree::LSMTree(int degree,
					 int levels,
					 const vector<int>& capacityPerLevel,
					 const string& name)
		: memoryTree(nullptr),
		  treeDegree(degree),
		  totalLevels(levels),
		  capacities(capacityPerLevel),
		  lsmName(name) {
		if (levels <= 0) {
			throw invalid_argument("LSMTree: levels must be positive");
		}
		if (capacities.size() != static_cast<size_t>(levels)) {
			throw invalid_argument("LSMTree: capacity list size must match levels");
		}
		if (capacities[0] <= 0) {
			throw invalid_argument("LSMTree: memory level capacity must be positive");
		}
		for (size_t i = 1; i < capacities.size(); ++i) {
			if (capacities[i] <= 0) {
				throw invalid_argument("LSMTree: disk level capacities must be positive");
			}
		}
		memoryTree = new BPlusTree<LSMTree::KeyType>(treeDegree);
		levelFilenames.resize(levels);
		for (int level = 1; level < levels; ++level) {
			levelFilenames[level] = buildFilename(level);
		}
	}
	
	LSMTree::~LSMTree() {
		if (memoryTree) {
			delete memoryTree;
			memoryTree = nullptr;
		}
	}
	
	void LSMTree::insert(LSMTree::KeyType key, LSMTree::ValueType value) {
		auto tombstoneIt = tombstones.find(key);
		if (tombstoneIt != tombstones.end()) {
			tombstones.erase(tombstoneIt);
		}
	
		memoryTree->insert(key, value);
		if (memoryTree->numOfElements > capacities[0]) {
			flushMemoryLevel();
		}
	}
	
	LSMTree::ValueType* LSMTree::search(LSMTree::KeyType key) {
		if (tombstones.count(key)) {
			return nullptr;
		}
	
		ValueType* inMemory = memoryTree->search(key);
		if (inMemory) {
			return inMemory;
		}
	
		for (int level = 1; level < totalLevels; ++level) {
			auto records = readLevelRecords(level);
			for (const auto& record : records) {
				if (record.first == key) {
					return new ValueType(record.second);
				}
			}
		}
		return nullptr;
	}
	
	bool LSMTree::remove(LSMTree::KeyType key) {
		ValueType* inMemory = memoryTree->search(key);
		if (inMemory) {
			memoryTree->remove(key);
			return true;
		}
	
		for (int level = 1; level < totalLevels; ++level) {
			auto records = readLevelRecords(level);
			auto it = find_if(records.begin(), records.end(),
							  [key](const Record& record) { return record.first == key; });
			if (it != records.end()) {
				tombstones.insert(key);
				return true;
			}
		}
		return false;
	}
	
	vector<LSMTree::Record> LSMTree::getAllRecords() {
		vector<Record> allRecords = memoryTree->getAllRecords();
		for (int level = 1; level < totalLevels; ++level) {
			auto levelRecords = readLevelRecords(level);
			allRecords.insert(allRecords.end(), levelRecords.begin(), levelRecords.end());
		}
		compactAndDeduplicate(allRecords);
		applyTombstones(allRecords);
		return allRecords;
	}
	
	int LSMTree::getTotalElements() const {
		int total = memoryTree->numOfElements;
		for (int level = 1; level < totalLevels; ++level) {
			const string& filename = levelFilenames[level];
			if (filename.empty() || !fs::exists(filename)) {
				continue;
			}
			auto records = BPlusTree<LSMTree::KeyType>(treeDegree).getFromFile(filename);
			if (records) {
				total += static_cast<int>(records->size());
				delete records;
			}
		}
		total -= static_cast<int>(tombstones.size());
		return max(total, 0);
	}
	
	int LSMTree::getMemoryElements() const {
		return memoryTree->numOfElements;
	}
	
	int LSMTree::getDiskFiles() const {
		int files = 0;
		for (int level = 1; level < totalLevels; ++level) {
			auto filename = levelFilenames[level];
			if (!filename.empty() && fs::exists(filename)) {
				++files;
			}
		}
		return files;
	}
	
	void LSMTree::flushMemoryLevel() {
		auto memoryRecords = memoryTree->getAllRecords();
		if (memoryRecords.empty()) {
			return;
		}
		resetMemoryTree();
		mergeIntoLevel(1, std::move(memoryRecords));
	}
	
	void LSMTree::mergeIntoLevel(int levelIndex, vector<Record>&& incoming) {
		if (incoming.empty()) {
			return;
		}
	
		if (levelIndex >= totalLevels) {
			levelIndex = totalLevels - 1;
		}
	
		auto existing = readLevelRecords(levelIndex);
		incoming.insert(incoming.end(), existing.begin(), existing.end());
	
		compactAndDeduplicate(incoming);
		applyTombstones(incoming);
	
		size_t capacity = capacities[levelIndex];
		if (levelIndex == totalLevels - 1 || incoming.size() <= capacity) {
			writeLevelRecords(levelIndex, incoming);
			return;
		}
	
		writeLevelRecords(levelIndex, {});
		mergeIntoLevel(levelIndex + 1, std::move(incoming));
	}
	
	vector<LSMTree::Record> LSMTree::readLevelRecords(int levelIndex) const {
		vector<Record> records;
		if (levelIndex <= 0 || levelIndex >= totalLevels) {
			return records;
		}
	
		string filename = levelFilenames[levelIndex];
		if (filename.empty() || !fs::exists(filename)) {
			return records;
		}
	
		auto rawRecords = BPlusTree<LSMTree::KeyType>(treeDegree).getFromFile(filename);
		if (rawRecords) {
			records.assign(rawRecords->begin(), rawRecords->end());
			delete rawRecords;
		}
		return records;
	}
	
	void LSMTree::writeLevelRecords(int levelIndex, const vector<Record>& records) {
		if (levelIndex <= 0 || levelIndex >= totalLevels) {
			return;
		}
	
		string filename = levelFilenames[levelIndex];
		if (records.empty()) {
			if (!filename.empty() && fs::exists(filename)) {
				fs::remove(filename);
			}
			return;
		}
	
		vector<Record> sortedRecords = records;
		sort(sortedRecords.begin(), sortedRecords.end(),
			 [](const Record& a, const Record& b) { return a.first < b.first; });
	
		BPlusTree<LSMTree::KeyType>* levelTree = new BPlusTree<LSMTree::KeyType>(treeDegree);
		levelTree->bottom_up(sortedRecords);
		if (!levelTree->writeToFile(filename)) {
			cerr << "Failed to write LSM level to " << filename << endl;
		}
		delete levelTree;
	}
	
	string LSMTree::buildFilename(int levelIndex) const {
		return "LSM_" + lsmName + "_LVL" + to_string(levelIndex) + ".bin";
	}
	
	void LSMTree::resetMemoryTree() {
		delete memoryTree;
		memoryTree = new BPlusTree<LSMTree::KeyType>(treeDegree);
	}
	
	void LSMTree::applyTombstones(vector<Record>& records) {
		if (tombstones.empty() || records.empty()) {
			return;
		}
		vector<KeyType> cleaned;
		auto newEnd = remove_if(records.begin(), records.end(),
								[this, &cleaned](const Record& record) {
									if (tombstones.count(record.first)) {
										cleaned.push_back(record.first);
										return true;
									}
									return false;
								});
		records.erase(newEnd, records.end());
		for (KeyType key : cleaned) {
			tombstones.erase(key);
		}
	}
	
	void LSMTree::compactAndDeduplicate(vector<Record>& records) {
		if (records.empty()) {
			return;
		}
		map<KeyType, ValueType> ordered;
		for (const auto& record : records) {
			ordered[record.first] = record.second;
		}
		records.clear();
		records.reserve(ordered.size());
		for (const auto& entry : ordered) {
			records.emplace_back(entry.first, entry.second);
		}
	}
	

