.PHONY: all files

LD_FLAGS=-lrt
X=
GCC=gcc -O3 -msse -msse2 -mssse3 -msse4.1 -msse4.2 -I../src
GCC=gcc -O3 -I../src
CC=$(GCC)

all: c/test_gj c/test_gj_memalign c/test_system cpp/test_gj cpp/test_fsb cpp/test_boost cpp/test_std cpp/list_gj cpp/list_boost cpp/list_std cpp/list_fsb
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

cpp:
	test -d cpp || mkdir cpp

c:
	test -d c || mkdir c

perf:
	test -d perf || mkdir perf

cpp/test_%:	 test_%.hh test.cpp cpp
	cat $< >> a.in.c
	echo "#line 1 \"test.cpp\"" >> a.in.c
	cat test.cpp >> a.in.c
	g++ $X -I../src -O3 -o $@ ../src/block_allocator.c a.in.c -lrt
	rm a.in.c
cpp/list_%:	 test_%.hh test.cpp cpp
	cat $< >> a.in.c
	echo "#line 1 \"list.cpp\"" >> a.in.c
	cat list.cpp >> a.in.c
	g++ $X -I../src -O3 -o $@ ../src/block_allocator.c a.in.c -lrt
	rm a.in.c
c/test_%:	test_%.h test.c  c
	cat $< >> a.in.c
	echo "#line 1 \"test.c\"" >> a.in.c
	cat test.c >> a.in.c
	gcc $X -I../src -O3 -o $@ ../src/block_allocator.c a.in.c -lrt
	rm a.in.c
perf/test_%.dat: cpp/test_%
	./$< > $@
	for i in `seq 1 3`; do ./$< >> $@; done
	pike compare.pike $@ > $@.tmp
	mv $@.tmp $@

perf/test.png: perf/test_gj.dat perf/test_std.dat perf/test_boost.dat perf/test_fsb.dat perf/test_rt.dat
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

perf/Linear.png: perf
	$(MAKE) perf/test.png -B
	mv perf/test.png perf/Linear.png
	

