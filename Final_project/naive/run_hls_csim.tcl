open_project naive_hls
open_solution solution1

# Compile & run the C/C++ test bench
# -argv passes the command-line parameters that spmv_tb.cpp expects:
#   2 iterations  ./data/tiny_matrix.mtx      (edit to suit your path)
csim_design -clean   -argv {2 /home/chingwen/Project/aahls/Final/SDSoC-Benchmarks/SpMV/spmvf_naive_sdx/data/tiny_matrix}

exit

