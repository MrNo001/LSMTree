



build:
	g++ -o bin/main src/*.cpp

test-skiplist:
	mkdir -p bin
	g++ -std=c++17 -o bin/skiplist_test src/SkipList.cpp tests/SkipListTest.cpp
	./bin/skiplist_test

test-lsm-store:
	mkdir -p bin
	g++ -std=c++17 -I h -o bin/lsm_smoke tests/LSMStoreSmoke.cpp src/SkipList.cpp src/SSTable.cpp src/Manifest.cpp src/LSMStore.cpp
	./bin/lsm_smoke

test-btree-store:
	mkdir -p bin
	g++ -std=c++17 -I h -o bin/btree_smoke tests/BTreeStoreSmoke.cpp src/BTreeStore.cpp
	./bin/btree_smoke

clean: 
	rm -f bin/main bin/skiplist_test bin/lsm_smoke bin/btree_smoke

run:
	./bin/main