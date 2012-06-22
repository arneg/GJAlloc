.PHONY: all files

LD_FLAGS=-lrt

FLAGS=-g -Wall -O0 -DBA_USE_VALGRIND
CFLAGS=$(FLAGS)
CXXFLAGS=$(FLAGS)

CC=gcc $(CFLAGS) $(X) $(LD_FLAGS)
CXX=g++ $(CXXFLAGS) $(X) $(LD_FLAGS)

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

