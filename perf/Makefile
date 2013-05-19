.PHONY: all files

LD_FLAGS=-lrt
X=
GCC=gcc -O3 -msse -msse2 -mssse3 -msse4.1 -msse4.2 -I../src
GCC=gcc -O3 -I../src
NUM=3

CC=gcc -fno-inline -ggdb -g -I../src -fstrict-aliasing -Wstrict-aliasing=2 -Wall -O3 -msse $(X) $(LD_FLAGS)
CC=gcc -I../src -fstrict-aliasing -Wstrict-aliasing=2 -Wall -O3 -msse $(X) $(LD_FLAGS)
CXX=g++ -g -Wall -O3 -msse $(X) $(LD_FLAGS)

all: c/test_gj c/test_gjlocal c/test_system cpp/test_gj cpp/test_fsb cpp/test_boost cpp/test_std c/test_cha cpp/test_rt c/test_gjnofree
	@echo ""
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


files: test.c ../src/gjalloc.c ../src/gjalloc.h alloc.o alloc2.o


alloc.o: ../src/gjalloc.c ../src/gjalloc.h Makefile
	$(CC) $X -o $@ -c $<
alloc2.o: ../src/gjalloc.c ../src/gjalloc.h Makefile
	$(CC) -DBA_USE_MEMALIGN $X -o $@ -c $<

cpp:
	test -d cpp || mkdir cpp

c:
	test -d c || mkdir c

perf:
	test -d perf || mkdir perf

c/test_%.c:	test_%.h test.c  c
	@cat $< > $@
	@echo "#line 1 \"test.c\"" >> $@
	@cat test.c >> $@

c/test_%:	c/test_%.c
	$(CC) $< -o $@

cpp/test_%.cpp:	 test_%.hh test.c cpp
	@cat $< > $@
	@echo "#line 1 \"test.c\"" >> $@
	@cat test.c >> $@

cpp/test_%: cpp/test_%.cpp
	$(CXX) $< -o $@

cpp/test_fsb: cpp/test_fsb.cpp
	(test -d FSBAlloc && $(CXX) -IFSBAlloc $< -o $@) || echo "FSBAlloc missing"

cpp/test_gj: cpp/test_gj.cpp
	$(CXX) -I../src ../src/gjalloc.c $< -o $@

cpp/test_rt: cpp/test_rt.cpp
	(test -d rtalloc && $(CXX) -Irtalloc rtalloc/rtAllocator.cpp $< -o $@) || echo "rtalloc missing"

c/test_gj: c/test_gj.c
	$(CC) -I../src ../src/gjalloc.c $< -o $@

c/test_gjlocal: c/test_gjlocal.c
	$(CC) -I../src ../src/gjalloc.c $< -o $@

c/test_gjdoubling: c/test_gjdoubling.c
	$(CC) -I../src ../src/gjalloc.c $< -o $@

c/test_gjlog: c/test_gjlog.c
	$(CC) -I../src ../src/gjalloc.c $< -o $@

c/test_gjtemporary: c/test_gjtemporary.c
	$(CC) -I../src ../src/gjalloc.c $< -o $@

c/test_gjnofree: c/test_gjnofree.c
	$(CC) -I../src ../src/gjalloc.c $< -o $@

c/test_cha: c/test_cha.c
	(test -d challoc && $(CC) -Ichalloc challoc/challoc.c $< -o $@) || echo "challoc missing"

cpp/list_%: test_%.hh test.cpp cpp
	cat $< > a.in.c
	echo "#line 1 \"list.cpp\"" >> a.in.c
	cat list.cpp >> a.in.c
	g++ $X -I../src -O3 -o $@ ../src/gjalloc.c a.in.c -lrt
	rm a.in.c

perf/test_%.dat: cpp/test_%
	./$< > $@
	for i in `seq 1 $(NUM)`; do ./$< >> $@; done
	pike compare.pike $@ > $@.tmp
	mv $@.tmp $@

perf/test_cha.dat: c/test_cha
	./$< > $@
	for i in `seq 1 $(NUM)`; do ./$< >> $@; done
	pike compare.pike $@ > $@.tmp
	mv $@.tmp $@

perf/test_system.dat: c/test_system
	./$< > $@
	for i in `seq 1 $(NUM)`; do ./$< >> $@; done
	pike compare.pike $@ > $@.tmp
	mv $@.tmp $@

perf/test.png: perf/test_gj.dat perf/test_std.dat perf/test_boost.dat perf/test_fsb.dat perf/test_rt.dat perf/test_cha.dat perf/test_system.dat
	cp test.plot test.plot.tmp
ifdef RANDOM
	sed -i -e s/__YMAX__/350/ test.plot.tmp
else
	sed -i -e s/__YMAX__/70/ test.plot.tmp
endif
	gnuplot test.plot.tmp
	rm test.plot.tmp

perf/Random.png: perf
	$(MAKE) RANDOM=1 X="$(X) -DRANDOM" perf/test.png -B
	mv perf/test.png perf/Random.png
	mv perf/test_mem.png perf/Random_mem.png

perf/Linear.png: perf
	$(MAKE) perf/test.png -B
	mv perf/test.png perf/Linear.png
	mv perf/test_mem.png perf/Linear_mem.png
