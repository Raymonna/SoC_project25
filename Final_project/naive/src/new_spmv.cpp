#include "spmv.h"
#include <hls_stream.h>

// -------------------------------------------------------------------
// Function to read from global memory (only one process reads memory)
// -------------------------------------------------------------------
static void read_data(
    DATA_TYPE  *values,
    u32        *cols,
    u32        *rows,
    hls::stream<DATA_TYPE> &values_fifo,
    hls::stream<u32>       &cols_fifo,
    hls::stream<u32>       &rows_fifo,
    u32 row_size,
    u32 data_size
) {
    // Read rows
    for (u32 i = 0; i < row_size; i++) {
    #pragma HLS PIPELINE II=1
        rows_fifo << rows[i];
    }

    // Read values and cols
    for (u32 i = 0; i < data_size; i++) {
    #pragma HLS PIPELINE II=1
        values_fifo << values[i];
        cols_fifo   << cols[i];
    }
}

// -------------------------------------------------------------------
// Function that does the SPMV multiply-accumulate
// -------------------------------------------------------------------
static void compute(
    hls::stream<DATA_TYPE> &values_fifo,
    hls::stream<u32>       &cols_fifo,
    hls::stream<u32>       &rows_fifo,
    hls::stream<DATA_TYPE> &results_fifo,
    DATA_TYPE *x_local,
    u32 row_size,
    u32 data_size
) {
    u32 col_left = 0;
    DATA_TYPE sum = 0;

    // For each nonzero element
    for (u32 r = 0; r < data_size; r++) {
    #pragma HLS PIPELINE II=1
        if (col_left == 0) {
            // read how many columns in the next row
            col_left = rows_fifo.read();
            sum = 0;
        }

        DATA_TYPE value = values_fifo.read();
        u32       col   = cols_fifo.read();
        sum += value * x_local[col];

        col_left--;
        if (col_left == 0) {
            // end of this row => push sum out
            results_fifo << sum;
        }
    }
}

// -------------------------------------------------------------------
// Function to write results back to global memory (only one process writes memory)
// -------------------------------------------------------------------
static void write_data(
    hls::stream<DATA_TYPE> &results_fifo,
    DATA_TYPE  *y,
    u32 row_size
) {
    for (u32 i = 0; i < row_size; i++) {
    #pragma HLS PIPELINE II=1
        y[i] = results_fifo.read();
    }
}



void spvm_kernel(
    DATA_TYPE  *values,
    u32        *cols,
    u32        *rows,
    DATA_TYPE  *x_local,
    DATA_TYPE  *y,
    u32         row_size,
    u32         col_size,
    u32         data_size
) {
#pragma HLS DATAFLOW

    // Declare local streams/FIFOs
    hls::stream<u32>       rows_fifo("rows_fifo");
    hls::stream<DATA_TYPE> values_fifo("values_fifo");
    hls::stream<u32>       cols_fifo("cols_fifo");
    hls::stream<DATA_TYPE> results_fifo("results_fifo");

    // Optionally set a custom depth to avoid deadlock warnings:
    #pragma HLS STREAM variable=rows_fifo    depth=64
    #pragma HLS STREAM variable=values_fifo  depth=64
    #pragma HLS STREAM variable=cols_fifo    depth=64
    #pragma HLS STREAM variable=results_fifo depth=64

    // (1) Read from memory into streams
    read_data(values, cols, rows,
              values_fifo,
              cols_fifo,
              rows_fifo,
              row_size,
              data_size);

    // (2) Do the actual spmv multiply-accumulate
    compute(values_fifo, cols_fifo, rows_fifo,
            results_fifo,
            x_local,
            row_size,
            data_size);

    // (3) Write results out
    write_data(results_fifo, y, row_size);
}
