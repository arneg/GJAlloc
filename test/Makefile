.PHONY: all files

LD_FLAGS=-lrt

CC=gcc -g -Wall -O0 -msse $(X) $(LD_FLAGS)
CXX=g++ -g -Wall -O3 -msse $(X) $(LD_FLAGS)

all: c/test_walk

c:
	(test -d c || mkdir c)

cpp:
	(test -d cpp || mkdir cpp)

files: ../src/gjalloc.c ../src/gjalloc.h c cpp

cpp/test_%: test_%.cpp files ../src/gjalloc.hh
	$(CXX) -I../src ../src/gjalloc.c $< -o $@

c/test_%: test_%.c files
	$(CC) -I../src ../src/gjalloc.c $< -o $@

