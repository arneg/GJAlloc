.PHONY: all files

LD_FLAGS=-lrt

FLAGS=-Wall -O3
DFLAGS=-DBA_USE_VALGRIND -DBA_DEBUG
CFLAGS=$(FLAGS)
CXXFLAGS=$(FLAGS)

CC=gcc $(CFLAGS) $(X) $(LD_FLAGS)
CXX=g++ $(CXXFLAGS) $(X) $(LD_FLAGS)

tests = $(foreach t, $(notdir $(basename $(wildcard test_*.c))), $(subst test_,,$(t)))
allocators = $(foreach t, $(notdir $(basename $(wildcard ../perf/test_*.h))), $(subst test_,,$(t)))

define TEMPLATE
c/$(1)_$(2): c/$(1)_$(2).c files
	$(CC) $(DFLAGS) -I../src ../src/gjalloc.c $$< -o $$@
c/$(1)_$(2).c: test_$(1).c ../perf/test_$(2).h c
	echo "#line 1 \"$$<\"" | cat ../perf/test_$(2).h - $$< > $$@
S/$(1)_$(2).S: c/$(1)_$(2).c files
	$(CC) -I../src -S $$< -o $$@
endef

test_programs = $(foreach allocator,$(allocators), $(foreach test,$(tests), c/$(test)_$(allocator)))

all: tests

tests: $(test_programs)

$(foreach allocator,$(allocators), $(eval $(foreach test,$(tests), $(call TEMPLATE,$(test),$(allocator)))))

files: ../src/gjalloc.c ../src/gjalloc.h c cpp S
 
c:
	(test -d c || mkdir c)

cpp:
	(test -d cpp || mkdir cpp)

S:
	(test -d S || mkdir S)

