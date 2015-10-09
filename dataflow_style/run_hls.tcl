
#passed
set top deflate259

open_project hls_proj -reset

#add_files deflate.c
#add_files lz77.c
add_files deflate_dataflow.cpp
add_files huffman_translate.cpp

#add_files compress_compare.c -tb;
add_files -tb zpipe.c
add_files -tb ex1.sam
add_files -tb sam_tree.bin
add_files -tb ex1.sam.gold.def0
#add_files -tb testbench.c

set_top ${top}

open_solution solution1
set_part xc7vx690tffg1157-2
create_clock -period 5ns

csim_design

csynth_design
#cosim_design -bc;#-trace_level all
#cosim_design -bc
cosim_design -trace_level all
#export_design -evaluate verilog

exit

