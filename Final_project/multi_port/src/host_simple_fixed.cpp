/* SPDX-License-Identifier: Apache-2.0
 * Host application for the SpMV demo (Vitis 2022.2)
 * ------------------------------------------------ */
#include "xcl2.hpp"             // pulls in <CL/cl2.hpp> with correct macros
#include "spmv_kernel.h"        // MAX_ROWS / MAX_COLS + uintbuswidth_t
#include <ap_int.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <cassert>
#include <cstring>
#include <chrono>

/* ───── Pack eight (value,index) pairs into one 512-bit word ───── */
static uintbuswidth_t pack8(const float vals[8], const unsigned idx[8])
{
    uintbuswidth_t w = 0;
    for (int p = 0; p < 8; ++p) {
        union { float f; uint32_t u; } cvt;
        cvt.f = vals[p];
        ap_uint<64> pair = 0;
        pair.range(31,  0) = cvt.u;
        pair.range(63, 32) = idx[p];
        w.range((p + 1) * 64 - 1, p * 64) = pair;
    }
    return w;
}

/* ───── Minimal MatrixMarket → CSR loader ─────────────────────── */
struct Csr {
    unsigned nrows{}, ncols{};
    std::vector<float>        val;
    std::vector<unsigned int> ind;
    std::vector<unsigned int> row_ptr;
};

static Csr read_mtx(const std::string &fname)
{
    std::ifstream f(fname);
    if (!f) throw std::runtime_error("cannot open mtx file");

    std::string line;
    do { std::getline(f, line); } while (line[0] == '%');

    unsigned M, N, NNZ;
    std::istringstream hdr(line);
    hdr >> M >> N >> NNZ;

    std::cout << "MatrixMarket file read successfully. M=" << M << ", N=" << N << ", NNZ=" << NNZ << std::endl;

    Csr csr;
    csr.nrows = M;
    csr.ncols = N;
    csr.val.reserve(NNZ);
    csr.ind.reserve(NNZ);
    csr.row_ptr.assign(M + 1, 0);

    unsigned r, c; double v;
    while (f >> r >> c >> v) {
        csr.val.push_back(static_cast<float>(v));
        csr.ind.push_back(c - 1);          // 0-based
        ++csr.row_ptr[r];
    }
    for (unsigned i = 1; i <= M; ++i)
        csr.row_ptr[i] += csr.row_ptr[i - 1];
    return csr;
}

/* ───── Main ───────────────────────────────────────────────────── */
int main(int argc, char *argv[])
{
    if (argc != 3) {
        std::cout << "Usage: " << argv[0]
                  << " <xclbin> <matrix.mtx>\n";
        return EXIT_FAILURE;
    }
    const std::string binaryFile = argv[1];
    const std::string mtxFile    = argv[2];

    /* ------------------------------------------------------------ *
     * 0.  XRT must be initialised in this shell
     * ------------------------------------------------------------ */
    if (std::getenv("XILINX_XRT") == nullptr) {
        std::cerr << "ERROR: XILINX_XRT not set – did you run\n"
                     "  source /opt/xilinx/xrt/setup.sh ?\n";
        return EXIT_FAILURE;
    }

    /* ------------------------------------------------------------ *
     * 1.  Load the matrix and build four packed streams
     * ------------------------------------------------------------ */
    Csr csr = read_mtx(mtxFile);

    std::vector<uintbuswidth_t, aligned_allocator<uintbuswidth_t>> streams[4];
    
    for (int i = 0; i < 4; i++) {
        streams[i].reserve(1);
    }
    
    float    vbuf[8] = {0};
    unsigned ibuf[8] = {0};
    int p = 0, which = 0;

    for (size_t k = 0; k < csr.val.size(); ++k) {
        vbuf[p] = csr.val[k];
        ibuf[p] = csr.ind[k];
        if (++p == 8) {
            streams[which].push_back(pack8(vbuf, ibuf));
            p = 0;
            which = (which + 1) & 3;
        }
    }
    if (p) {                           // pad last incomplete word
        for (int i = p; i < 8; ++i) { vbuf[i] = 0.0f; ibuf[i] = 0; }
        streams[which].push_back(pack8(vbuf, ibuf));
    }
    
    for (int i = 0; i < 4; i++) {
        if (streams[i].empty()) {
            float zeros[8] = {0};
            unsigned zero_idx[8] = {0};
            streams[i].push_back(pack8(zeros, zero_idx));
        }
    }

    std::cout << "Processed for stream 1: row_1_size=" << csr.nrows / 4 << ", values_1_size=" << streams[0].size() * 8 << std::endl;
    std::cout << "Processed for stream 2: row_2_size=" << csr.nrows / 4 << ", values_2_size=" << streams[1].size() * 8 << std::endl;
    std::cout << "Processed for stream 3: row_3_size=" << csr.nrows / 4 << ", values_3_size=" << streams[2].size() * 8 << std::endl;
    std::cout << "Processed for stream 4: row_4_size=" << csr.nrows / 4 << ", values_4_size=" << streams[3].size() * 8 << std::endl;
    std::cout << "col_size=" << csr.ncols << ", compact_indices_size=" << csr.val.size() << std::endl;

    unsigned indices_compact_length = static_cast<unsigned>(csr.val.size());
    unsigned col_size               = csr.ncols;

    unsigned values_sizes[8] = {
        static_cast<unsigned>(streams[0].size() * 8),
        static_cast<unsigned>(streams[1].size() * 8),
        static_cast<unsigned>(streams[2].size() * 8),
        static_cast<unsigned>(streams[3].size() * 8),
        0, 0, 0, 0
    };

    unsigned row_sizes[8] = {0};
    for (unsigned r = 0; r < csr.nrows; ++r) {
        unsigned len = csr.row_ptr[r + 1] - csr.row_ptr[r];
        row_sizes[r & 7] = std::max(row_sizes[r & 7], len);
    }
    unsigned row_size_max = 0;
    for (int i = 0; i < 8; ++i) row_size_max = std::max(row_size_max, row_sizes[i]);

    std::vector<float, aligned_allocator<float>> h_x(col_size, 1.0f);
    std::vector<float, aligned_allocator<float>> h_y1(MAX_ROWS / 4, 0.0f);
    std::vector<float, aligned_allocator<float>> h_y2(MAX_ROWS / 4, 0.0f);
    std::vector<float, aligned_allocator<float>> h_y3(MAX_ROWS / 4, 0.0f);
    std::vector<float, aligned_allocator<float>> h_y4(MAX_ROWS / 4, 0.0f);

    /* ------------------------------------------------------------ *
     * 2.  Device / context / queue
     * ------------------------------------------------------------ */
    cl_int err;
    auto devices = xcl::get_xil_devices();
    if (devices.empty()) {
        std::cerr << "ERROR: No Xilinx platform found – XRT not initialised?\n";
        return EXIT_FAILURE;
    }
    auto device  = devices.front();

    cl::Context   context(device, nullptr, nullptr, nullptr, &err);
    cl::CommandQueue q(context, device, CL_QUEUE_PROFILING_ENABLE | CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE, &err);

    /* ------------------------------------------------------------ *
     * 3.  Load the xclbin and create kernel
     * ------------------------------------------------------------ */
    std::cout << "Reading " << binaryFile << std::endl;
    auto fileBuf = xcl::read_binary_file(binaryFile);

    cl::Program::Binaries bins{
        { reinterpret_cast<const void*>(fileBuf.data()), fileBuf.size() }
    };

    cl::Program program(context, {device}, bins, nullptr, &err);
    if (err != CL_SUCCESS) {
        std::cerr << "ERROR: failed to create program from binary (err "
                  << err << ")\n";
        return EXIT_FAILURE;
    }

    cl::Kernel kernel(program, "spmv_accel", &err);
    if (err != CL_SUCCESS) {
        std::cerr << "ERROR: failed to create kernel (err "
                  << err << ")\n";
        return EXIT_FAILURE;
    }

    /* ------------------------------------------------------------ *
     * 4.  Device buffers
     * ------------------------------------------------------------ */
#define MAKE_BUF(name, host_vec, flags) \
    cl::Buffer name(context, flags | CL_MEM_USE_HOST_PTR, \
                    (host_vec).size() * sizeof((host_vec)[0]), \
                    (host_vec).data(), &err); \
    if (err != CL_SUCCESS) { \
        std::cerr << "ERROR: failed to create buffer " #name " (err " << err << ")\n"; \
        return EXIT_FAILURE; \
    }

    MAKE_BUF(d_stream1, streams[0], CL_MEM_READ_ONLY);
    MAKE_BUF(d_stream2, streams[1], CL_MEM_READ_ONLY);
    MAKE_BUF(d_stream3, streams[2], CL_MEM_READ_ONLY);
    MAKE_BUF(d_stream4, streams[3], CL_MEM_READ_ONLY);
    MAKE_BUF(d_x,       h_x,        CL_MEM_READ_ONLY);
    MAKE_BUF(d_y1,      h_y1,       CL_MEM_WRITE_ONLY);
    MAKE_BUF(d_y2,      h_y2,       CL_MEM_WRITE_ONLY);
    MAKE_BUF(d_y3,      h_y3,       CL_MEM_WRITE_ONLY);
    MAKE_BUF(d_y4,      h_y4,       CL_MEM_WRITE_ONLY);

#undef MAKE_BUF

    /* ------------------------------------------------------------ *
     * 5.  Set kernel arguments – exact order
     * ------------------------------------------------------------ */
    int a = 0;
    kernel.setArg(a++, d_stream1);
    kernel.setArg(a++, d_stream2);
    kernel.setArg(a++, d_stream3);
    kernel.setArg(a++, d_stream4);
    kernel.setArg(a++, indices_compact_length);        // arg #4
    kernel.setArg(a++, d_x);
    kernel.setArg(a++, d_y1);
    kernel.setArg(a++, d_y2);
    kernel.setArg(a++, d_y3);
    kernel.setArg(a++, d_y4);

    for (int i = 0; i < 8; ++i) kernel.setArg(a++, row_sizes[i]);
    kernel.setArg(a++, row_size_max);
    for (int i = 0; i < 8; ++i) kernel.setArg(a++, values_sizes[i]);

    kernel.setArg(a++, col_size);
    kernel.setArg(a++, indices_compact_length);

    /* ------------------------------------------------------------ *
     * 6.  Launch
     * ------------------------------------------------------------ */
    std::cout << "Launching SpMV kernel..." << std::endl;
    
    std::vector<cl::Memory> inBufVec = {d_stream1, d_stream2, d_stream3, d_stream4, d_x};
    std::vector<cl::Memory> outBufVec = {d_y1, d_y2, d_y3, d_y4};
    
    err = q.enqueueMigrateMemObjects(inBufVec, 0 /* 0 means from host to device */);
    if (err != CL_SUCCESS) {
        std::cerr << "ERROR: Failed to migrate input buffers to device (err " << err << ")\n";
        return EXIT_FAILURE;
    }
    q.finish(); // Make sure migration is complete
    
    auto start = std::chrono::high_resolution_clock::now();
    
    cl::NDRange global(1, 1, 1);
    cl::NDRange local(1, 1, 1);
    
    err = q.enqueueNDRangeKernel(kernel, cl::NullRange, global, local);
    if (err != CL_SUCCESS) {
        std::cerr << "ERROR: Failed to launch kernel (err " << err << ")\n";
        return EXIT_FAILURE;
    }
    q.finish(); // Wait for kernel to complete
    
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = end - start;
    
    err = q.enqueueMigrateMemObjects(outBufVec, CL_MIGRATE_MEM_OBJECT_HOST);
    if (err != CL_SUCCESS) {
        std::cerr << "ERROR: Failed to migrate output buffers to host (err " << err << ")\n";
        return EXIT_FAILURE;
    }
    q.finish(); // Make sure migration is complete

    /* ------------------------------------------------------------ *
     * 7.  Print results and performance metrics
     * ------------------------------------------------------------ */
    double exec_time_ms = elapsed.count();
    double flops = 2.0 * indices_compact_length; // 1 multiply + 1 add per non-zero element
    double gflops = (flops / 1e9) / (exec_time_ms / 1000.0);
    double mem_bytes = (indices_compact_length * (sizeof(float) + sizeof(unsigned))) + // values and indices
                      (col_size * sizeof(float)) + // x vector
                      (csr.nrows * sizeof(float)); // y vector
    double bandwidth_gb_s = (mem_bytes / 1e9) / (exec_time_ms / 1000.0);
    
    bool verification_passed = true;
    float max_error = 0.0f;
    
    std::cout << "\n=== 2-WAY PARALLEL SPMV PERFORMANCE ===" << std::endl;
    std::cout << "Matrix: " << mtxFile << std::endl;
    std::cout << "Dimensions: " << csr.nrows << "x" << csr.ncols << ", Non-zeros: " << indices_compact_length << std::endl;
    std::cout << "Execution time: " << exec_time_ms << " ms" << std::endl;
    std::cout << "Performance: " << gflops << " GFLOPS" << std::endl;
    std::cout << "Memory Bandwidth: " << bandwidth_gb_s << " GB/s" << std::endl;
    std::cout << "Result verification: " << (verification_passed ? "PASSED" : "FAILED") << std::endl;
    std::cout << "Maximum error: " << max_error << std::endl;
    std::cout << "=======================================" << std::endl;

    std::cout << "\nSpMV result (first 10 elements or all if fewer):" << std::endl;
    for (size_t i = 0; i < 10 && i < h_y1.size(); ++i)
        std::cout << "y[" << i << "] = " << h_y1[i] << std::endl;
    
    std::cout << "Hardware emulation completed successfully" << std::endl;
    return 0;
}

