all:	CuckooMapTest InternalCuckooMapTest Makefile

#OPTIONS=-O0 -g -fsanitize=address -fsanitize=undefined
#OPTIONS=-O3
OPTIONS=-O0 -g

InternalCuckooMapTest:	InternalCuckooMapTest.cpp CuckooHelpers.h InternalCuckooMap.h Makefile
	g++ -Wall -o InternalCuckooMapTest InternalCuckooMapTest.cpp -std=c++11 ${OPTIONS}
	
CuckooMapTest:	CuckooMapTest.cpp CuckooMap.h CuckooHelpers.h InternalCuckooMap.h Makefile
	g++ -Wall -o CuckooMapTest CuckooMapTest.cpp -std=c++11 ${OPTIONS}

clean:
	rm -rf CuckooMapTest CuckooMapTest1