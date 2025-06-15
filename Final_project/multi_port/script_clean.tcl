open_project spmv_u280_hls_clean
set_top spmv_accel
add_files src/spmv_kernel_simple_fixed_standalone.cpp  
add_files src/spmv_kernel.h
add_files -tb src/spmv_tb.cpp
open_solution "solution1" -flow_target vitis
set_part xcu280-fsvh2892-2L-e
create_clock -period 3.33 -name default
config_export -format xo -rtl verilog -output ./spmv_kernel_simple.xo
csynth_design
export_design
exit
