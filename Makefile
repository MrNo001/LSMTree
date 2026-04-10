



build:
	g++ -o bin/main src/*.cpp

test-skiplist:
	mkdir -p bin
	g++ -std=c++17 -o bin/skiplist_test src/SkipList.cpp tests/SkipListTest.cpp
	./bin/skiplist_test

clean: 
	rm -f bin/main bin/skiplist_test

run:
	./bin/main