set terminal png
set output "perf/test.png"
set ylabel "alloc+free in ns"
set yrange [0:__YMAX__]
set xlabel "number of blocks used"
set style data errorlines
set key left top
set log x
plot "perf/test_gj.dat" title "GJAlloc", "perf/test_std.dat" title "std:allocator",\
     "perf/test_boost.dat" title "boost::fast_pool_allocator",\
     "perf/test_fsb.dat" title "FSBAllocator",\
     "perf/test_rt.dat" title "rtAlloator"
