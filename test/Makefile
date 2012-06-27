.PHONY: all files

LD_FLAGS=-lrt

FLAGS=-g -Wall -O0 -DBA_USE_VALGRIND -DBA_DEBUG
CFLAGS=$(FLAGS)
CXXFLAGS=$(FLAGS)

CC=gcc $(CFLAGS) $(X) $(LD_FLAGS)
CXX=g++ $(CXXFLAGS) $(X) $(LD_FLAGS)

tests = $(foreach t, $(notdir $(basename $(wildcard test_*.c))), $(subst test_,,$(t)))
allocators = $(foreach t, $(notdir $(basename $(wildcard ../perf/test_*.h))), $(subst test_,,$(t)))

define TEMPLATE
c/$(1)_$(2): c/$(1)_$(2).c files
	$(CC) -I../src ../src/gjalloc.c $$< -o $$@
c/$(1)_$(2).c: test_$(1).c ../perf/test_$(2).h c
	echo "#line 1 \"$$<\"" | cat ../perf/test_$(2).h - $$< > $$@
endef

test_programs = $(foreach allocator,$(allocators), $(foreach test,$(tests), c/$(test)_$(allocator)))

all: tests

tests: $(test_programs)

$(foreach allocator,$(allocators), $(eval $(foreach test,$(tests), $(call TEMPLATE,$(test),$(allocator)))))

files: ../src/gjalloc.c ../src/gjalloc.h c cpp
 
c:
	(test -d c || mkdir c)

cpp:
	(test -d cpp || mkdir cpp)
