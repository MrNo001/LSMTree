#ifndef LSMTREE_HPP
#define LSMTREE_HPP

#include <string>
#include <unordered_set>
#include <vector>

#include "../src/BTree.cpp"

		class LSMTree {
public:
		using KeyType = int;
    using ValueType = typename BPlusTree<KeyType>::ValueType;
    using Record = typename BPlusTree<KeyType>::Record;

    LSMTree(int degree,
            int levels,
            const std::vector<int>& capacityPerLevel,
            const std::string& name);
    ~LSMTree();

		void insert(KeyType key, ValueType value);
		ValueType* search(KeyType key);
		bool remove(KeyType key);
		std::vector<Record> getAllRecords();
    int getTotalElements() const;
    int getMemoryElements() const;
    int getDiskFiles() const;

private:
    void flushMemoryLevel();
		void mergeIntoLevel(int levelIndex, std::vector<Record>&& incoming);
		std::vector<Record> readLevelRecords(int levelIndex) const;
		void writeLevelRecords(int levelIndex, const std::vector<Record>& records);
    std::string buildFilename(int levelIndex) const;
    void resetMemoryTree();
		void applyTombstones(std::vector<Record>& records);
		void compactAndDeduplicate(std::vector<Record>& records);

		BPlusTree<KeyType>* memoryTree;
    int treeDegree;
    int totalLevels;
    std::vector<int> capacities;
    std::vector<std::string> levelFilenames;
    std::string lsmName;
		std::unordered_set<KeyType> tombstones;
};
#endif // LSMTREE_HPP

