open_project  naive_hls
open_solution solution1

#csynth_design                        ;# generate RTL
cosim_design \
   -tool xsim \
   -rtl  verilog \
   -argv {2 /home/chingwen/Project/aahls/Final/SDSoC-Benchmarks/SpMV/spmvf_naive_sdx/data/tiny_matrix} \
   -trace_level all \
   -enable_fifo_sizing
   #-wave_debug \

exit

