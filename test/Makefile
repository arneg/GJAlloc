.PHONY: all files

LD_FLAGS=-lrt
X=
GCC=gcc -O3 -msse -msse2 -mssse3 -msse4.1 -msse4.2 -I../src
GCC=gcc -O3 -I../src
CC=$(GCC)

all: ba_segregate ba_system alloc.o alloc2.o ba_dsegregate
	@echo ""
	@echo ./ba_system to benchmark system malloc
	@echo ./ba_segregate to benchmark block allocator with hash
	@echo ./ba_dsegregate to benchmark block allocator with aligned pages
ifndef X
	@echo ""
	@echo Run as
	@echo make X="-DSOMETHING" to set predefines or compiler flags.
	@echo
	@echo define RANDOM to benchmark a non-linear case.
	@echo
	@echo See also: cat test.c
endif

clean:
	- rm -f alloc.o alloc2.o ba_system ba_segregate ba_dsegregate


files: test.c ../src/block_allocator.c ../src/block_allocator.h alloc.o alloc2.o


alloc.o: ../src/block_allocator.c ../src/block_allocator.h Makefile
	$(CC) $X -o $@ -c $<
alloc2.o: ../src/block_allocator.c ../src/block_allocator.h Makefile
	$(CC) -DBA_USE_MEMALIGN $X -o $@ -c $<

ba_segregate: files
	${CC} $(LD_FLAGS) $X -DBA_SEGREGATE -o $@ test.c alloc.o

ba_dsegregate: files
	${CC} $(LD_FLAGS) $X -DBA_SEGREGATE -DBA_USE_MEMALIGN -o $@ test.c alloc2.o

ba_system: files
	${CC} $(LD_FLAGS) -DSYSTEM_ALLOC -o $@ test.c

cpp/test_%:	 test_%.hh test.cpp
	cat $< > a.in.c
	echo "#line 1 \"test.cpp\"" >> a.in.c
	cat test.cpp >> a.in.c
	g++ $X -I../src -O3 -o $@ ../src/block_allocator.c a.in.c -lrt
	rm a.in.c
cpp/list_%:	 test_%.hh test.cpp
	cat $< > a.in.c
	echo "#line 1 \"test.cpp\"" >> a.in.c
	cat list.cpp >> a.in.c
	g++ $X -I../src -O3 -o $@ ../src/block_allocator.c a.in.c -lrt
	rm a.in.c
c/test_%:	test_%.h test.c 
	cat $< > a.in.c
	echo "#line 1 \"test.c\"" >> a.in.c
	cat test.c >> a.in.c
	gcc $X -I../src -O3 -o $@ ../src/block_allocator.c a.in.c -lrt
	rm a.in.c
