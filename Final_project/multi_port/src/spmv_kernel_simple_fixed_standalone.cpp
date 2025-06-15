/* SPDX-License-Identifier: Apache-2.0
 * Stand-alone SpMV kernel – bundle-name fix for Vitis flow
 * ------------------------------------------------------- */
#include "spmv_kernel.h"
#include <hls_math.h>

/* ───── Helper to unpack one (value,index) pair ──────────────── */
static inline void unpack_pair(uintbuswidth_t word,
                               int            pair_idx,
                               float         &val,
                               unsigned      &idx)
{
#pragma HLS INLINE
    const int lo = pair_idx * 64;
    ap_uint<64> slice = word.range(lo + 63, lo);
    ap_uint<32> val_bits = slice.range(31,  0);
    ap_uint<32> idx_bits = slice.range(63, 32);

    union { uint32_t u; float f; } cvt;
    cvt.u = val_bits.to_uint();
    val   = cvt.f;
    idx   = idx_bits.to_uint();
}

/* ─────────────────────────────────────────────────────────────── *
 * TOP FUNCTION
 * ─────────────────────────────────────────────────────────────── */
extern "C"
void spmv_accel(
        uintbuswidth_t *values_indices_1,
        uintbuswidth_t *values_indices_2,
        uintbuswidth_t *values_indices_3,
        uintbuswidth_t *values_indices_4,
        unsigned        indices_compact_length,
        float          *x,
        float          *y_1,
        float          *y_2,
        float          *y_3,
        float          *y_4,
        unsigned        row_1_size,
        unsigned        row_2_size,
        unsigned        row_3_size,
        unsigned        row_4_size,
        unsigned        row_5_size,
        unsigned        row_6_size,
        unsigned        row_7_size,
        unsigned        row_8_size,
        unsigned        row_size_max,
        unsigned        values_1_size,
        unsigned        values_2_size,
        unsigned        values_3_size,
        unsigned        values_4_size,
        unsigned        values_5_size,
        unsigned        values_6_size,
        unsigned        values_7_size,
        unsigned        values_8_size,
        unsigned        col_size,
        unsigned        compact_indices_size)
{
/* ───── AXI HBM / DDR ports ──────────────────────────────────── */
#pragma HLS INTERFACE m_axi port=values_indices_1 offset=slave bundle=gmem0 depth=MAX_NNZ
#pragma HLS INTERFACE m_axi port=values_indices_2 offset=slave bundle=gmem1 depth=MAX_NNZ
#pragma HLS INTERFACE m_axi port=values_indices_3 offset=slave bundle=gmem2 depth=MAX_NNZ
#pragma HLS INTERFACE m_axi port=values_indices_4 offset=slave bundle=gmem3 depth=MAX_NNZ
#pragma HLS INTERFACE m_axi port=x                offset=slave bundle=gmem4 depth=MAX_COLS
#pragma HLS INTERFACE m_axi port=y_1              offset=slave bundle=gmem5 depth=MAX_ROWS/4
#pragma HLS INTERFACE m_axi port=y_2              offset=slave bundle=gmem6 depth=MAX_ROWS/4
#pragma HLS INTERFACE m_axi port=y_3              offset=slave bundle=gmem7 depth=MAX_ROWS/4
#pragma HLS INTERFACE m_axi port=y_4              offset=slave bundle=gmem8 depth=MAX_ROWS/4

/* ───── AXI-Lite control registers  (***all on bundle=control***) */
#pragma HLS INTERFACE s_axilite port=values_indices_1 bundle=control
#pragma HLS INTERFACE s_axilite port=values_indices_2 bundle=control
#pragma HLS INTERFACE s_axilite port=values_indices_3 bundle=control
#pragma HLS INTERFACE s_axilite port=values_indices_4 bundle=control
#pragma HLS INTERFACE s_axilite port=x                bundle=control
#pragma HLS INTERFACE s_axilite port=y_1              bundle=control
#pragma HLS INTERFACE s_axilite port=y_2              bundle=control
#pragma HLS INTERFACE s_axilite port=y_3              bundle=control
#pragma HLS INTERFACE s_axilite port=y_4              bundle=control

#pragma HLS INTERFACE s_axilite port=indices_compact_length bundle=control

#pragma HLS INTERFACE s_axilite port=row_1_size bundle=control
#pragma HLS INTERFACE s_axilite port=row_2_size bundle=control
#pragma HLS INTERFACE s_axilite port=row_3_size bundle=control
#pragma HLS INTERFACE s_axilite port=row_4_size bundle=control
#pragma HLS INTERFACE s_axilite port=row_5_size bundle=control
#pragma HLS INTERFACE s_axilite port=row_6_size bundle=control
#pragma HLS INTERFACE s_axilite port=row_7_size bundle=control
#pragma HLS INTERFACE s_axilite port=row_8_size bundle=control
#pragma HLS INTERFACE s_axilite port=row_size_max bundle=control

#pragma HLS INTERFACE s_axilite port=values_1_size bundle=control
#pragma HLS INTERFACE s_axilite port=values_2_size bundle=control
#pragma HLS INTERFACE s_axilite port=values_3_size bundle=control
#pragma HLS INTERFACE s_axilite port=values_4_size bundle=control
#pragma HLS INTERFACE s_axilite port=values_5_size bundle=control
#pragma HLS INTERFACE s_axilite port=values_6_size bundle=control
#pragma HLS INTERFACE s_axilite port=values_7_size bundle=control
#pragma HLS INTERFACE s_axilite port=values_8_size bundle=control

#pragma HLS INTERFACE s_axilite port=col_size            bundle=control
#pragma HLS INTERFACE s_axilite port=compact_indices_size bundle=control
#pragma HLS INTERFACE s_axilite port=return               bundle=control

/* ───── On-chip scratchpad for partial sums ───────────────────── */
    float y_local[MAX_ROWS / 4];
#pragma HLS bind_storage variable=y_local type=ram_1p impl=bram
#pragma HLS ARRAY_PARTITION variable=y_local cyclic factor=16 dim=1

 /* --- Initialise ------------------------------------------------ */
init_loop:
    for (int i = 0; i < MAX_ROWS / 4; ++i) {
#pragma HLS PIPELINE II=1
        y_local[i] = 0.0f;
    }

 /* --- Very small demo compute core (stream-1 only) -------------- */
compute_loop:
    for (unsigned w = 0; w < values_1_size; ++w) {
#pragma HLS PIPELINE II=1
        uintbuswidth_t word = values_indices_1[w];

        for (int p = 0; p < VALUES_PER_BUS_WORD; ++p) {
#pragma HLS UNROLL
            float    v;  unsigned j;
            unpack_pair(word, p, v, j);

            if (j < MAX_COLS) {
                unsigned row = j;          // trivial mapping
                if (row < MAX_ROWS / 4)
                    y_local[row] += v * x[j];
            }
        }
    }

 /* --- Write-back ------------------------------------------------ */
wb_loop:
    for (int i = 0; i < MAX_ROWS / 4; ++i) {
#pragma HLS PIPELINE II=1
        y_1[i] = y_local[i];
        y_2[i] = 0.0f;
        y_3[i] = 0.0f;
        y_4[i] = 0.0f;
    }
}

