/**
 * Host code for SpMV U280 hardware emulation
 * Fixed to match the standalone kernel interface
 */
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <chrono>
#include <iomanip>
#include "xcl2.hpp"
#include "spmv_kernel.h"

bool readMTXFile(const std::string& filename,
                 std::vector<int>& row_indices,
                 std::vector<int>& col_indices,
                 std::vector<float>& values,
                 int& M, int& N, int& nnz) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << filename << std::endl;
        return false;
    }

    std::string line;
    do {
        std::getline(file, line);
    } while (line[0] == '%');

    std::istringstream iss(line);
    iss >> M >> N >> nnz;

    row_indices.resize(nnz);
    col_indices.resize(nnz);
    values.resize(nnz);

    for (int i = 0; i < nnz; i++) {
        std::getline(file, line);
        std::istringstream entry_iss(line);
        int row, col;
        float val;
        entry_iss >> row >> col >> val;

        row_indices[i] = row - 1;
        col_indices[i] = col - 1;
        values[i] = val;
    }

    file.close();
    std::cout << "MatrixMarket file read successfully. M=" << M << ", N=" << N << ", NNZ=" << nnz << std::endl;
    return true;
}

void prepareData(const std::vector<int>& row_indices,
                 const std::vector<int>& col_indices,
                 const std::vector<float>& values,
                 int M, int N, int nnz,
                 std::vector<ap_uint<128>>& values_indices_1,
                 std::vector<ap_uint<128>>& values_indices_2,
                 std::vector<ap_uint<128>>& values_indices_3,
                 std::vector<ap_uint<128>>& values_indices_4,
                 int& row_1_size, int& row_2_size, int& row_3_size, int& row_4_size,
                 int& values_1_size, int& values_2_size, int& values_3_size, int& values_4_size) {

    row_1_size = (M + 1) / 2;
    row_2_size = M - row_1_size;
    row_3_size = 0;
    row_4_size = 0;

    values_1_size = 0;
    values_2_size = 0;
    values_3_size = 0;
    values_4_size = 0;

    for (int i = 0; i < nnz; i++) {
        if (row_indices[i] < row_1_size) {
            values_1_size++;
        } else {
            values_2_size++;
        }
    }

    values_indices_1.resize((values_1_size + 3) / 4 * 4); // Align to 4
    values_indices_2.resize((values_2_size + 3) / 4 * 4); // Align to 4
    values_indices_3.resize(4); // Dummy buffer
    values_indices_4.resize(4); // Dummy buffer

    int idx1 = 0, idx2 = 0;
    for (int i = 0; i < nnz; i++) {
        union float_ap_uint32_t val;
        val.f = values[i];

        ap_uint<128> entry = 0;
        entry.range(31, 0) = col_indices[i];
        entry.range(63, 32) = val.apint;
        entry.range(95, 64) = row_indices[i];

        if (row_indices[i] < row_1_size) {
            values_indices_1[idx1++] = entry;
        } else {
            values_indices_2[idx2++] = entry;
        }
    }

    std::cout << "Processed for stream 1: row_1_size=" << row_1_size
              << ", values_1_size=" << values_1_size << std::endl;
    std::cout << "Processed for stream 2: row_2_size=" << row_2_size
              << ", values_2_size=" << values_2_size << std::endl;
}

std::vector<float> spmv_reference(
    const std::vector<int>& row_indices,
    const std::vector<int>& col_indices,
    const std::vector<float>& values,
    int M, int N, int nnz) {

    std::vector<float> x(N, 1.0f); // Input vector (all ones)
    std::vector<float> y(M, 0.0f); // Output vector

    for (int i = 0; i < nnz; i++) {
        y[row_indices[i]] += values[i] * x[col_indices[i]];
    }

    return y;
}

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cout << "Usage: " << argv[0] << " <XCLBIN File> <MTX File>" << std::endl;
        return EXIT_FAILURE;
    }

    std::string binaryFile = argv[1];
    std::string matrixFile = argv[2];

    std::vector<int> row_indices, col_indices;
    std::vector<float> values;
    int M, N, nnz;

    if (!readMTXFile(matrixFile, row_indices, col_indices, values, M, N, nnz)) {
        return EXIT_FAILURE;
    }

    std::vector<ap_uint<128>> values_indices_1, values_indices_2, values_indices_3, values_indices_4;
    int row_1_size, row_2_size, row_3_size, row_4_size;
    int values_1_size, values_2_size, values_3_size, values_4_size;

    prepareData(row_indices, col_indices, values, M, N, nnz,
                values_indices_1, values_indices_2, values_indices_3, values_indices_4,
                row_1_size, row_2_size, row_3_size, row_4_size,
                values_1_size, values_2_size, values_3_size, values_4_size);

    std::vector<ap_uint<128>> x((N + 3) / 4);
    for (int i = 0; i < (N + 3) / 4; i++) {
        union float_ap_uint32_t tmp1, tmp2, tmp3, tmp4;
        tmp1.f = 1.0f;
        tmp2.f = 1.0f;
        tmp3.f = 1.0f;
        tmp4.f = 1.0f;

        ap_uint<128> entry = 0;
        entry.range(31, 0) = tmp1.apint;
        entry.range(63, 32) = tmp2.apint;
        entry.range(95, 64) = tmp3.apint;
        entry.range(127, 96) = tmp4.apint;

        x[i] = entry;
    }

    std::vector<ap_uint<128>> y_1((row_1_size + 3) / 4);
    std::vector<ap_uint<128>> y_2((row_2_size + 3) / 4);
    std::vector<ap_uint<128>> y_3(4); // Dummy buffer
    std::vector<ap_uint<128>> y_4(4); // Dummy buffer

    int indices_compact_length = nnz;
    int row_size_max = std::max({row_1_size, row_2_size, row_3_size, row_4_size});
    int row_5_size = 0, row_6_size = 0, row_7_size = 0, row_8_size = 0;
    int values_5_size = 0, values_6_size = 0, values_7_size = 0, values_8_size = 0;

    std::cout << "col_size=" << N << ", compact_indices_size=" << indices_compact_length << std::endl;

    cl_int err;
    cl::Context context;
    cl::CommandQueue q;
    cl::Kernel kernel;

    auto devices = xcl::get_xil_devices();
    auto device = devices[0];

    OCL_CHECK(err, context = cl::Context(device, nullptr, nullptr, nullptr, &err));
    OCL_CHECK(err, q = cl::CommandQueue(context, device, CL_QUEUE_PROFILING_ENABLE, &err));

    auto fileBuf = xcl::read_binary_file(binaryFile);
    cl::Program::Binaries bins{{fileBuf.data(), fileBuf.size()}};

    OCL_CHECK(err, cl::Program program(context, {device}, bins, nullptr, &err));
    OCL_CHECK(err, kernel = cl::Kernel(program, "spmv_accel", &err));

    OCL_CHECK(err, cl::Buffer buffer_values_indices_1(context, CL_MEM_USE_HOST_PTR | CL_MEM_READ_ONLY,
                                                    sizeof(ap_uint<128>) * values_indices_1.size(),
                                                    values_indices_1.data(), &err));

    OCL_CHECK(err, cl::Buffer buffer_values_indices_2(context, CL_MEM_USE_HOST_PTR | CL_MEM_READ_ONLY,
                                                    sizeof(ap_uint<128>) * values_indices_2.size(),
                                                    values_indices_2.data(), &err));

    OCL_CHECK(err, cl::Buffer buffer_values_indices_3(context, CL_MEM_USE_HOST_PTR | CL_MEM_READ_ONLY,
                                                    sizeof(ap_uint<128>) * values_indices_3.size(),
                                                    values_indices_3.data(), &err));

    OCL_CHECK(err, cl::Buffer buffer_values_indices_4(context, CL_MEM_USE_HOST_PTR | CL_MEM_READ_ONLY,
                                                    sizeof(ap_uint<128>) * values_indices_4.size(),
                                                    values_indices_4.data(), &err));

    OCL_CHECK(err, cl::Buffer buffer_x(context, CL_MEM_USE_HOST_PTR | CL_MEM_READ_ONLY,
                                      sizeof(ap_uint<128>) * x.size(),
                                      x.data(), &err));

    OCL_CHECK(err, cl::Buffer buffer_y_1(context, CL_MEM_USE_HOST_PTR | CL_MEM_WRITE_ONLY,
                                        sizeof(ap_uint<128>) * y_1.size(),
                                        y_1.data(), &err));

    OCL_CHECK(err, cl::Buffer buffer_y_2(context, CL_MEM_USE_HOST_PTR | CL_MEM_WRITE_ONLY,
                                        sizeof(ap_uint<128>) * y_2.size(),
                                        y_2.data(), &err));

    OCL_CHECK(err, cl::Buffer buffer_y_3(context, CL_MEM_USE_HOST_PTR | CL_MEM_WRITE_ONLY,
                                        sizeof(ap_uint<128>) * y_3.size(),
                                        y_3.data(), &err));

    OCL_CHECK(err, cl::Buffer buffer_y_4(context, CL_MEM_USE_HOST_PTR | CL_MEM_WRITE_ONLY,
                                        sizeof(ap_uint<128>) * y_4.size(),
                                        y_4.data(), &err));

    int arg_idx = 0;
    OCL_CHECK(err, err = kernel.setArg(arg_idx++, buffer_values_indices_1));
    OCL_CHECK(err, err = kernel.setArg(arg_idx++, buffer_values_indices_2));
    OCL_CHECK(err, err = kernel.setArg(arg_idx++, buffer_values_indices_3));
    OCL_CHECK(err, err = kernel.setArg(arg_idx++, buffer_values_indices_4));
    OCL_CHECK(err, err = kernel.setArg(arg_idx++, buffer_x));
    OCL_CHECK(err, err = kernel.setArg(arg_idx++, buffer_y_1));
    OCL_CHECK(err, err = kernel.setArg(arg_idx++, buffer_y_2));
    OCL_CHECK(err, err = kernel.setArg(arg_idx++, buffer_y_3));
    OCL_CHECK(err, err = kernel.setArg(arg_idx++, buffer_y_4));
    OCL_CHECK(err, err = kernel.setArg(arg_idx++, row_1_size));
    OCL_CHECK(err, err = kernel.setArg(arg_idx++, row_2_size));
    OCL_CHECK(err, err = kernel.setArg(arg_idx++, row_3_size));
    OCL_CHECK(err, err = kernel.setArg(arg_idx++, row_4_size));
    OCL_CHECK(err, err = kernel.setArg(arg_idx++, row_5_size));
    OCL_CHECK(err, err = kernel.setArg(arg_idx++, row_6_size));
    OCL_CHECK(err, err = kernel.setArg(arg_idx++, row_7_size));
    OCL_CHECK(err, err = kernel.setArg(arg_idx++, row_8_size));
    OCL_CHECK(err, err = kernel.setArg(arg_idx++, row_size_max));
    OCL_CHECK(err, err = kernel.setArg(arg_idx++, values_1_size));
    OCL_CHECK(err, err = kernel.setArg(arg_idx++, values_2_size));
    OCL_CHECK(err, err = kernel.setArg(arg_idx++, values_3_size));
    OCL_CHECK(err, err = kernel.setArg(arg_idx++, values_4_size));
    OCL_CHECK(err, err = kernel.setArg(arg_idx++, values_5_size));
    OCL_CHECK(err, err = kernel.setArg(arg_idx++, values_6_size));
    OCL_CHECK(err, err = kernel.setArg(arg_idx++, values_7_size));
    OCL_CHECK(err, err = kernel.setArg(arg_idx++, values_8_size));
    OCL_CHECK(err, err = kernel.setArg(arg_idx++, N)); // col_size
    OCL_CHECK(err, err = kernel.setArg(arg_idx++, indices_compact_length));

    OCL_CHECK(err, err = q.enqueueMigrateMemObjects({buffer_values_indices_1, buffer_values_indices_2, 
                                                    buffer_values_indices_3, buffer_values_indices_4, 
                                                    buffer_x}, 0));

    std::cout << "Launching SpMV kernel..." << std::endl;

    auto start_time = std::chrono::high_resolution_clock::now();

    OCL_CHECK(err, err = q.enqueueTask(kernel));

    OCL_CHECK(err, err = q.finish());

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);

    OCL_CHECK(err, err = q.enqueueMigrateMemObjects({buffer_y_1, buffer_y_2, buffer_y_3, buffer_y_4}, CL_MIGRATE_MEM_OBJECT_HOST));
    OCL_CHECK(err, err = q.finish());

    std::vector<float> result(M, 0.0f);

    for (int i = 0; i < (row_1_size + 3) / 4; i++) {
        ap_uint<128> tmp = y_1[i];

        union float_ap_uint32_t tmp1, tmp2, tmp3, tmp4;
        tmp1.apint = tmp.range(31, 0);
        tmp2.apint = tmp.range(63, 32);
        tmp3.apint = tmp.range(95, 64);
        tmp4.apint = tmp.range(127, 96);

        int idx = i * 4;
        if (idx < row_1_size) result[idx] = tmp1.f;
        if (idx + 1 < row_1_size) result[idx + 1] = tmp2.f;
        if (idx + 2 < row_1_size) result[idx + 2] = tmp3.f;
        if (idx + 3 < row_1_size) result[idx + 3] = tmp4.f;
    }

    for (int i = 0; i < (row_2_size + 3) / 4; i++) {
        ap_uint<128> tmp = y_2[i];

        union float_ap_uint32_t tmp1, tmp2, tmp3, tmp4;
        tmp1.apint = tmp.range(31, 0);
        tmp2.apint = tmp.range(63, 32);
        tmp3.apint = tmp.range(95, 64);
        tmp4.apint = tmp.range(127, 96);

        int idx = i * 4 + row_1_size;
        if (idx < M) result[idx] = tmp1.f;
        if (idx + 1 < M) result[idx + 1] = tmp2.f;
        if (idx + 2 < M) result[idx + 2] = tmp3.f;
        if (idx + 3 < M) result[idx + 3] = tmp4.f;
    }

    double execution_time_ms = duration.count() / 1000.0;
    double gflops = (2.0 * nnz) / (execution_time_ms * 1e-3) / 1e9;  // GFLOPS
    double bandwidth_gb_s = (sizeof(float) * (nnz + N + M)) / (execution_time_ms * 1e-3) / 1e9;  // GB/s

    std::vector<float> reference = spmv_reference(row_indices, col_indices, values, M, N, nnz);

    bool correct = true;
    double max_error = 0.0;
    for (int i = 0; i < M; i++) {
        double error = std::abs(result[i] - reference[i]);
        max_error = std::max(max_error, error);
        if (error > 1e-4) {
            correct = false;
            std::cout << "Error at index " << i << ": " << result[i] << " vs " << reference[i] << std::endl;
        }
    }

    std::cout << "\n=== 2-WAY PARALLEL SPMV PERFORMANCE ===" << std::endl;
    std::cout << "Matrix: " << matrixFile << std::endl;
    std::cout << "Dimensions: " << M << "x" << N << ", Non-zeros: " << nnz << std::endl;
    std::cout << "Execution time: " << std::fixed << std::setprecision(3) << execution_time_ms << " ms" << std::endl;
    std::cout << "Performance: " << std::fixed << std::setprecision(3) << gflops << " GFLOPS" << std::endl;
    std::cout << "Memory Bandwidth: " << std::fixed << std::setprecision(3) << bandwidth_gb_s << " GB/s" << std::endl;
    std::cout << "Result verification: " << (correct ? "PASSED" : "FAILED") << std::endl;
    std::cout << "Maximum error: " << std::scientific << std::setprecision(6) << max_error << std::endl;
    std::cout << "=======================================" << std::endl;

    std::cout << "\nSpMV result (first 10 elements or all if fewer):" << std::endl;
    for (int i = 0; i < std::min(10, M); i++) {
        std::cout << "y[" << i << "] = " << result[i] << std::endl;
    }

    std::cout << "Hardware emulation completed successfully" << std::endl;
    return 0;
}

