/* SPDX-License-Identifier: Apache-2.0
 * Shared SpMV kernel header (host + HLS)
 * ─────────────────────────────────────────────────────────────── */
#ifndef SPMV_KERNEL_H
#define SPMV_KERNEL_H

#include <ap_int.h>

/* ---------------------------------------------------------------- *
 * You may tune these constants for your design / test-bench.
 * ---------------------------------------------------------------- */
#define MAX_ROWS      4096
#define MAX_COLS      4096
#define MAX_NNZ       (MAX_ROWS * 16)   // rough upper bound
#define BUS_WIDTH     512               // bits
#define VALUES_PER_BUS_WORD  (BUS_WIDTH / 64)   // 8×(value,index) pairs
typedef ap_uint<BUS_WIDTH> uintbuswidth_t;

/* ---------------------------------------------------------------- *
 * Top-level prototype – **must exactly match the host side**
 * ---------------------------------------------------------------- */
extern "C"
void spmv_accel(
    /* Four 512-bit streams that hold packed (value,index) pairs. */
    uintbuswidth_t *values_indices_1,
    uintbuswidth_t *values_indices_2,
    uintbuswidth_t *values_indices_3,
    uintbuswidth_t *values_indices_4,

    /* NEW scalar – fixes the runtime argument mismatch. */
    unsigned int     indices_compact_length,

    /* Dense input vector  x  and output vector  y .*/
    float           *x,
    float           *y_1,
    float           *y_2,
    float           *y_3,
    float           *y_4,

    /* Row lengths split 8-way for streaming. */
    unsigned int     row_1_size,
    unsigned int     row_2_size,
    unsigned int     row_3_size,
    unsigned int     row_4_size,
    unsigned int     row_5_size,
    unsigned int     row_6_size,
    unsigned int     row_7_size,
    unsigned int     row_8_size,
    unsigned int     row_size_max,

    /* Per-stream NNZ counts (not strictly needed by this demo core,
       but preserved so your original host code still compiles).      */
    unsigned int     values_1_size,
    unsigned int     values_2_size,
    unsigned int     values_3_size,
    unsigned int     values_4_size,
    unsigned int     values_5_size,
    unsigned int     values_6_size,
    unsigned int     values_7_size,
    unsigned int     values_8_size,

    /* Matrix dimensions. */
    unsigned int     col_size,
    unsigned int     compact_indices_size   /* = indices_compact_length */
);

#endif /* SPMV_KERNEL_H */

