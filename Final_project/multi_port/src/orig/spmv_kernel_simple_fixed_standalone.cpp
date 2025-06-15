/**
 * SpMV kernel implementation for U280 hardware emulation
 * 2-way parallel version with consistent bundle names
 */
#include "spmv_kernel.h"
#include <hls_stream.h>

void compute_spmv(
    hls::stream<uintbuswidth_t>& values_stream,
    hls::stream<uintbuswidth_t>& x_stream,
    hls::stream<uintbuswidth_t>& y_stream,
    hls::stream<uintbuswidth_t>& y_out_stream,
    unsigned int row_1_size,
    unsigned int row_2_size,
    unsigned int row_3_size,
    unsigned int row_4_size,
    unsigned int row_5_size,
    unsigned int row_6_size,
    unsigned int row_7_size,
    unsigned int row_8_size,
    unsigned int values_size,
    unsigned int col_size
);

void spmv_accel(
    uintbuswidth_t  *values_indices_1,
    uintbuswidth_t  *values_indices_2,
    uintbuswidth_t  *values_indices_3,
    uintbuswidth_t  *values_indices_4,
    uintbuswidth_t  *x,
    uintbuswidth_t  *y_1,
    uintbuswidth_t  *y_2,
    uintbuswidth_t  *y_3,
    uintbuswidth_t  *y_4,
    unsigned int    row_1_size,
    unsigned int    row_2_size,
    unsigned int    row_3_size,
    unsigned int    row_4_size,
    unsigned int    row_5_size,
    unsigned int    row_6_size,
    unsigned int    row_7_size,
    unsigned int    row_8_size,
    unsigned int    row_size_max,
    unsigned int    values_1_size,
    unsigned int    values_2_size,
    unsigned int    values_3_size,
    unsigned int    values_4_size,
    unsigned int    values_5_size,
    unsigned int    values_6_size,
    unsigned int    values_7_size,
    unsigned int    values_8_size,
    unsigned int    col_size,
    unsigned int    compact_indices_size
) {
    #pragma HLS INTERFACE m_axi port=values_indices_1 bundle=gmem0 offset=slave depth=2500000
    #pragma HLS INTERFACE s_axilite port=values_indices_1 bundle=control
    
    #pragma HLS INTERFACE m_axi port=values_indices_2 bundle=gmem1 offset=slave depth=2500000
    #pragma HLS INTERFACE s_axilite port=values_indices_2 bundle=control
    
    #pragma HLS INTERFACE m_axi port=values_indices_3 bundle=gmem2 offset=slave depth=2500000
    #pragma HLS INTERFACE s_axilite port=values_indices_3 bundle=control
    
    #pragma HLS INTERFACE m_axi port=values_indices_4 bundle=gmem3 offset=slave depth=2500000
    #pragma HLS INTERFACE s_axilite port=values_indices_4 bundle=control
    
    #pragma HLS INTERFACE m_axi port=x bundle=gmem4 offset=slave depth=30000
    #pragma HLS INTERFACE s_axilite port=x bundle=control
    
    #pragma HLS INTERFACE m_axi port=y_1 bundle=gmem5 offset=slave depth=7500
    #pragma HLS INTERFACE s_axilite port=y_1 bundle=control
    
    #pragma HLS INTERFACE m_axi port=y_2 bundle=gmem6 offset=slave depth=7500
    #pragma HLS INTERFACE s_axilite port=y_2 bundle=control
    
    #pragma HLS INTERFACE m_axi port=y_3 bundle=gmem7 offset=slave depth=7500
    #pragma HLS INTERFACE s_axilite port=y_3 bundle=control
    
    #pragma HLS INTERFACE m_axi port=y_4 bundle=gmem8 offset=slave depth=7500
    #pragma HLS INTERFACE s_axilite port=y_4 bundle=control
    
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
    #pragma HLS INTERFACE s_axilite port=col_size bundle=control
    #pragma HLS INTERFACE s_axilite port=compact_indices_size bundle=control
    #pragma HLS INTERFACE s_axilite port=return bundle=control
    
    #pragma HLS DATAFLOW
    
    hls::stream<uintbuswidth_t> values_stream_1;
    #pragma HLS STREAM variable=values_stream_1 depth=32
    
    hls::stream<uintbuswidth_t> values_stream_2;
    #pragma HLS STREAM variable=values_stream_2 depth=32
    
    hls::stream<uintbuswidth_t> x_stream_1;
    #pragma HLS STREAM variable=x_stream_1 depth=32
    
    hls::stream<uintbuswidth_t> x_stream_2;
    #pragma HLS STREAM variable=x_stream_2 depth=32
    
    hls::stream<uintbuswidth_t> y_stream_1;
    #pragma HLS STREAM variable=y_stream_1 depth=32
    
    hls::stream<uintbuswidth_t> y_stream_2;
    #pragma HLS STREAM variable=y_stream_2 depth=32
    
    hls::stream<uintbuswidth_t> y_out_stream_1;
    #pragma HLS STREAM variable=y_out_stream_1 depth=32
    
    hls::stream<uintbuswidth_t> y_out_stream_2;
    #pragma HLS STREAM variable=y_out_stream_2 depth=32
    
    LOAD_VALUES_1: for (int i = 0; i < values_1_size; i++) {
        #pragma HLS PIPELINE II=1
        values_stream_1.write(values_indices_1[i]);
    }
    
    LOAD_VALUES_2: for (int i = 0; i < values_2_size; i++) {
        #pragma HLS PIPELINE II=1
        values_stream_2.write(values_indices_2[i]);
    }
    
    int x_size = (col_size + 3) / 4;
    LOAD_X_1: for (int i = 0; i < x_size; i++) {
        #pragma HLS PIPELINE II=1
        uintbuswidth_t x_val = x[i];
        x_stream_1.write(x_val);
        x_stream_2.write(x_val);
    }
    
    int y1_size = (row_1_size + 3) / 4;
    int y2_size = (row_2_size + 3) / 4;
    
    INIT_Y_1: for (int i = 0; i < y1_size; i++) {
        #pragma HLS PIPELINE II=1
        y_stream_1.write(0);
    }
    
    INIT_Y_2: for (int i = 0; i < y2_size; i++) {
        #pragma HLS PIPELINE II=1
        y_stream_2.write(0);
    }
    
    compute_spmv(
        values_stream_1,
        x_stream_1,
        y_stream_1,
        y_out_stream_1,
        row_1_size,
        row_2_size,
        row_3_size,
        row_4_size,
        row_5_size,
        row_6_size,
        row_7_size,
        row_8_size,
        values_1_size,
        col_size
    );
    
    compute_spmv(
        values_stream_2,
        x_stream_2,
        y_stream_2,
        y_out_stream_2,
        row_1_size,
        row_2_size,
        row_3_size,
        row_4_size,
        row_5_size,
        row_6_size,
        row_7_size,
        row_8_size,
        values_2_size,
        col_size
    );
    
    WRITE_Y_1: for (int i = 0; i < y1_size; i++) {
        #pragma HLS PIPELINE II=1
        y_1[i] = y_out_stream_1.read();
    }
    
    WRITE_Y_2: for (int i = 0; i < y2_size; i++) {
        #pragma HLS PIPELINE II=1
        y_2[i] = y_out_stream_2.read();
    }
}

void compute_spmv(
    hls::stream<uintbuswidth_t>& values_stream,
    hls::stream<uintbuswidth_t>& x_stream,
    hls::stream<uintbuswidth_t>& y_stream,
    hls::stream<uintbuswidth_t>& y_out_stream,
    unsigned int row_1_size,
    unsigned int row_2_size,
    unsigned int row_3_size,
    unsigned int row_4_size,
    unsigned int row_5_size,
    unsigned int row_6_size,
    unsigned int row_7_size,
    unsigned int row_8_size,
    unsigned int values_size,
    unsigned int col_size
) {
    float x_1_local[MAX_COL_SIZE];
    #pragma HLS ARRAY_PARTITION variable=x_1_local cyclic factor=2
    
    float x_2_local[MAX_COL_SIZE];
    #pragma HLS ARRAY_PARTITION variable=x_2_local cyclic factor=2
    
    float y_1_local[MAX_ROW_SIZE];
    #pragma HLS ARRAY_PARTITION variable=y_1_local cyclic factor=2
    
    float y_2_local[MAX_ROW_SIZE];
    #pragma HLS ARRAY_PARTITION variable=y_2_local cyclic factor=2
    
    float y_3_local[MAX_ROW_SIZE];
    float y_4_local[MAX_ROW_SIZE];
    float y_5_local[MAX_ROW_SIZE];
    float y_6_local[MAX_ROW_SIZE];
    float y_7_local[MAX_ROW_SIZE];
    float y_8_local[MAX_ROW_SIZE];
    
    int x_size = (col_size + 3) / 4;
    LOAD_X_LOCAL: for (int i = 0; i < x_size; i++) {
        #pragma HLS PIPELINE II=1
        uintbuswidth_t x_packed = x_stream.read();
        
        union float_ap_uint32_t tmp1, tmp2, tmp3, tmp4;
        tmp1.apint = x_packed.range(31, 0);
        tmp2.apint = x_packed.range(63, 32);
        tmp3.apint = x_packed.range(95, 64);
        tmp4.apint = x_packed.range(127, 96);
        
        int idx = i * 4;
        if (idx < col_size) x_1_local[idx] = tmp1.f;
        if (idx + 1 < col_size) x_1_local[idx + 1] = tmp2.f;
        if (idx + 2 < col_size) x_1_local[idx + 2] = tmp3.f;
        if (idx + 3 < col_size) x_1_local[idx + 3] = tmp4.f;
        
        if (idx < col_size) x_2_local[idx] = tmp1.f;
        if (idx + 1 < col_size) x_2_local[idx + 1] = tmp2.f;
        if (idx + 2 < col_size) x_2_local[idx + 2] = tmp3.f;
        if (idx + 3 < col_size) x_2_local[idx + 3] = tmp4.f;
    }
    
    int y_size = (row_1_size + 3) / 4;
    LOAD_Y_LOCAL: for (int i = 0; i < y_size; i++) {
        #pragma HLS PIPELINE II=1
        uintbuswidth_t y_packed = y_stream.read();
        
        union float_ap_uint32_t tmp1, tmp2, tmp3, tmp4;
        tmp1.apint = y_packed.range(31, 0);
        tmp2.apint = y_packed.range(63, 32);
        tmp3.apint = y_packed.range(95, 64);
        tmp4.apint = y_packed.range(127, 96);
        
        int idx = i * 4;
        if (idx < row_1_size) y_1_local[idx] = tmp1.f;
        if (idx + 1 < row_1_size) y_1_local[idx + 1] = tmp2.f;
        if (idx + 2 < row_1_size) y_1_local[idx + 2] = tmp3.f;
        if (idx + 3 < row_1_size) y_1_local[idx + 3] = tmp4.f;
    }
    
    INIT_Y2_LOCAL: for (int i = 0; i < row_2_size; i++) {
        #pragma HLS PIPELINE II=1
        y_2_local[i] = 0.0f;
    }
    
    PROCESS_ELEMENTS: for (int i = 0; i < values_size; i++) {
        #pragma HLS PIPELINE II=1
        
        uintbuswidth_t packed_data = values_stream.read();
        
        unsigned int col_idx = packed_data.range(31, 0);
        
        union float_ap_uint32_t tmp_val;
        tmp_val.apint = packed_data.range(63, 32);
        float val = tmp_val.f;
        
        unsigned int row_idx = packed_data.range(95, 64);
        
        if (row_idx < row_1_size) {
            y_1_local[row_idx] += val * x_1_local[col_idx];
        } else {
            y_2_local[row_idx - row_1_size] += val * x_2_local[col_idx];
        }
    }
    
    int y1_size = (row_1_size + 3) / 4;
    WRITE_Y1_STREAM: for (int i = 0; i < y1_size; i++) {
        #pragma HLS PIPELINE II=1
        
        uintbuswidth_t y_packed = 0;
        union float_ap_uint32_t tmp1, tmp2, tmp3, tmp4;
        
        int idx = i * 4;
        tmp1.f = (idx < row_1_size) ? y_1_local[idx] : 0.0f;
        tmp2.f = (idx + 1 < row_1_size) ? y_1_local[idx + 1] : 0.0f;
        tmp3.f = (idx + 2 < row_1_size) ? y_1_local[idx + 2] : 0.0f;
        tmp4.f = (idx + 3 < row_1_size) ? y_1_local[idx + 3] : 0.0f;
        
        y_packed.range(31, 0) = tmp1.apint;
        y_packed.range(63, 32) = tmp2.apint;
        y_packed.range(95, 64) = tmp3.apint;
        y_packed.range(127, 96) = tmp4.apint;
        
        y_out_stream.write(y_packed);
    }
    
    int y2_size = (row_2_size + 3) / 4;
    WRITE_Y2_STREAM: for (int i = 0; i < y2_size; i++) {
        #pragma HLS PIPELINE II=1
        
        uintbuswidth_t y_packed = 0;
        union float_ap_uint32_t tmp1, tmp2, tmp3, tmp4;
        
        int idx = i * 4;
        tmp1.f = (idx < row_2_size) ? y_2_local[idx] : 0.0f;
        tmp2.f = (idx + 1 < row_2_size) ? y_2_local[idx + 1] : 0.0f;
        tmp3.f = (idx + 2 < row_2_size) ? y_2_local[idx + 2] : 0.0f;
        tmp4.f = (idx + 3 < row_2_size) ? y_2_local[idx + 3] : 0.0f;
        
        y_packed.range(31, 0) = tmp1.apint;
        y_packed.range(63, 32) = tmp2.apint;
        y_packed.range(95, 64) = tmp3.apint;
        y_packed.range(127, 96) = tmp4.apint;
        
        y_out_stream.write(y_packed);
    }
}

