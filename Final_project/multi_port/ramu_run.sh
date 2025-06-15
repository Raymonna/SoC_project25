vitis_hls -f script_clean.tcl
v++ -l -t hw_emu --platform xilinx_u280_gen3x16_xdma_1_202211_1 --config connectivity_simple.cfg spmv_kernel_simple.xo -o spmv_simple.xclbin
make -f Makefile_unified host_simple
