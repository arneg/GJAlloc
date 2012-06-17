set terminal png
set output "perf/test.png"
set ylabel "alloc+free in ns"
set yrange [0:__YMAX__]
set xlabel "number of blocks used"
set style data errorlines
set key left top
set log x
plot "perf/test_gj.dat" using 1:2:3 title "GJAlloc",\
     "perf/test_std.dat" using 1:2:3 title "std::allocator",\
     "perf/test_boost.dat" using 1:2:3 title "boost::fast_pool_allocator",\
     "perf/test_fsb.dat" using 1:2:3 title "FSBAllocator",\
     "perf/test_cha.dat" using 1:2:3 title "challoc",\
     "perf/test_rt.dat" using 1:2:3 title "rtAllocator",\
     "perf/test_system.dat" using 1:2:3 title "system malloc"
set output "perf/test_mem.png"
set ylabel "memory usage in bytes per block / 32"
set yrange [0.9:*]
set log y
set key right top
set style data lines
plot "perf/test_gj.dat" using 1:($4/$1/32) title "GJAlloc",\
     "perf/test_std.dat" using 1:($4/$1/32) title "std::allocator",\
     "perf/test_boost.dat" using 1:($4/$1/32) title "boost::fast_pool_allocator",\
     "perf/test_fsb.dat" using 1:($4/$1/32) title "FSBAllocator",\
     "perf/test_cha.dat" using 1:($4/$1/32) title "challoc",\
     "perf/test_rt.dat" using 1:($4/$1/32) title "rtAllocator",\
     "perf/test_system.dat" using 1:($4/$1/32) title "system malloc"
