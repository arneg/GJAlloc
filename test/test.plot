set terminal png
set output "perf/test.png"
set ylabel "alloc+free in ns"
set xlabel "number of blocks used"
set style data linespoints
set log x
plot "perf/test_gj.dat" title "GJAlloc", "perf/test_std.dat" title "std:allocator",\
     "perf/test_boost.dat" title "boost::fast_pool_allocator",\
     "perf/test_fsb.dat" title "FSBAllocator"
