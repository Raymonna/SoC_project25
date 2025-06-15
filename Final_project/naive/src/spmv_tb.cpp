// =========================================================================================
// == FINAL VERSION of spmv_tb.cpp                                                        ==
// == 1. Removes all file I/O by using hardcoded data.                                  ==
// == 2. Fixes memory allocation to prevent buffer overflows (SIGSEGV error).             ==
// =========================================================================================

#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <iostream>
#include "spmv.h" // This header should provide ROWS, COLS, DATA_LENGTH
#include "time.h"

#ifdef __SDSCC__
  #include "sds_lib.h"
#else
  #include <cstdlib>
  #define sds_alloc_non_cacheable(sz)  malloc(sz)
  #define sds_free(ptr)                free(ptr)
#endif

#include <sys/time.h>

// =========================================================================================
// == HARDCODED MATRIX DATA                                                               ==
// =========================================================================================
const u32 G_ROW_SIZE_ACTUAL = 5;
const u32 G_COL_SIZE_ACTUAL = 5;
const u32 G_DATA_SIZE_ACTUAL = 10;

static const u32 G_ROW_NNZ[5] = {2, 2, 2, 2, 2};
static const u32   G_COLS[10]   = {1, 4, 2, 3, 0, 1, 3, 4, 0, 1};
static const DATA_TYPE G_VALUES[10] = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0};


double getTimestamp() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return tv.tv_usec + tv.tv_sec*1e6;
}

double hardware_start;
double hardware_end;
double hardware_execution_time;


// =========================================================================================
// == MODIFIED SpMV_Ref (GOLDEN C MODEL)                                                  ==
// =========================================================================================
void SpMV_Ref(
    DATA_TYPE *values,
    u32 *cols,
    u32 *rows_nnz,
    u32 row_size,
    u32 col_size,
    u32 data_size,
    DATA_TYPE *y
) {
    DATA_TYPE *x = (DATA_TYPE*)malloc(col_size * sizeof(DATA_TYPE));
    for (u32 i = 0; i < col_size; i++) {
        x[i] = i;
    }

    int i = 0, j = 0;
    int last_j = 0;
    for (i = 0; i < row_size; ++i) {
        DATA_TYPE y0 = 0.0;
        int new_index_j = last_j + rows_nnz[i];
        for (j = last_j; j < new_index_j; ++j) {
            y0 += values[j] * x[cols[j]];
        }
        last_j = last_j + rows_nnz[i];
        y[i] = y0;
    }
    free(x);
}

// =========================================================================================
// == MODIFIED spmv_init with CORRECT MEMORY ALLOCATION SIZES                             ==
// =========================================================================================
void spmv_init( DATA_TYPE **values, DATA_TYPE **x, u32 **cols, u32 **rows, u32 *row_size, u32 *col_size, u32 *data_size) {
    printf("check point spmv_init:01 (using hardcoded data and correct allocation size)\n");

    // Set the ACTUAL sizes for use in loops and calculations
    *row_size = G_ROW_SIZE_ACTUAL;
    *col_size = G_COL_SIZE_ACTUAL;
    *data_size = G_DATA_SIZE_ACTUAL;

    std::cout << "\r ACTUAL row_size = " << *row_size << ", MAX KERNEL size (ROWS) = " << ROWS << std::endl;
    std::cout << "\r ACTUAL col_size = " << *col_size << ", MAX KERNEL size (COLS) = " << COLS << std::endl;
    std::cout << "\r ACTUAL data_size = " << *data_size << ", MAX KERNEL size (DATA_LENGTH) = " << DATA_LENGTH << std::endl;
    
    // CRITICAL FIX: Allocate memory using the MAXIMUM sizes defined for the kernel
    *x       = (DATA_TYPE*)sds_alloc_non_cacheable(COLS * sizeof(DATA_TYPE));
    *rows    = (u32*)sds_alloc_non_cacheable(ROWS * sizeof(u32));
    *values  = (DATA_TYPE*)sds_alloc_non_cacheable(DATA_LENGTH * sizeof(DATA_TYPE));
    *cols    = (u32*)sds_alloc_non_cacheable(DATA_LENGTH * sizeof(u32));

    if(*x == NULL || *rows == NULL || *values == NULL || *cols == NULL) {
        printf("Error! memory not allocated.");
        exit(0);
    }
    
    // Copy the hardcoded data into the newly allocated buffers
    for (u32 i = 0; i < *row_size; i++) {
        (*rows)[i] = G_ROW_NNZ[i];
    }
    for (u32 i = 0; i < *data_size; i++) {
        (*values)[i] = G_VALUES[i];
        (*cols)[i] = G_COLS[i];
    }

    // Initialize the input vector 'x'
    for (u32 i = 0; i < *col_size; i++) {
        (*x)[i] = i;
    }
}


// =========================================================================================
// == MODIFIED main                                                                       ==
// =========================================================================================
int main(int argc, char **argv) {
    int status = 0;
    DATA_TYPE *x = NULL, *y_sw = NULL, *y_hw = NULL, *values = NULL;
    u32       *rows = NULL, *cols = NULL;
    u32       row_size, col_size, data_size;

    int no_iterations = (argc < 2) ? 2 : atoi(argv[1]);
    
    spmv_init( &values, &x, &cols, &rows, &row_size, &col_size, &data_size);
    
    printf("\rHello MM!\n\r");

    // Allocate memory for software and hardware output vectors
    y_sw = (DATA_TYPE*)malloc(row_size * sizeof(DATA_TYPE));
    y_hw = (DATA_TYPE*)sds_alloc_non_cacheable(ROWS * sizeof(DATA_TYPE)); // Use MAX size for HW output

    SpMV_Ref(values, cols, rows, row_size, col_size, data_size, y_sw);

    printf("\rHardware version started!\n\r");
    hardware_start = getTimestamp();
    spmv_accel(values, cols, rows, x, y_hw, row_size, col_size, data_size);
    hardware_end = getTimestamp();

    hardware_execution_time = (hardware_end-hardware_start)/(1000);
    printf("first time hardware execution time  %.6f ms elapsed\n", hardware_execution_time);

    // ... (rest of the main function is identical) ...
    
    printf("\rHardware version started!\n\r");
    hardware_start = getTimestamp();
    for (int i = 0; i < no_iterations; i++) {
        spmv_accel(values, cols, rows, x, y_hw, row_size, col_size, data_size);
    }
    hardware_end = getTimestamp();

    hardware_execution_time = (hardware_end-hardware_start)/(1000*no_iterations);
    printf("average hardware execution time  %.6f ms elapsed\n", hardware_execution_time);

    for(u32 i=0;i<row_size;i++) {
        DATA_TYPE diff = fabs(y_sw[i]-y_hw[i]);
        if(diff > 0.0001 || diff != diff){
            std::cout << "error occurs at " << i << " with value y_ hw = " <<  y_hw[i] << ", should be y_ sw = " << y_sw[i] << std::endl;
            status = -1;
            break;
        }
    }
    if(!status) {
        printf("Validation PASSED!\n");
    } else {
        printf("Validation FAILED!\n");
    }

    sds_free(x);
    sds_free(rows);
    sds_free(values);
    sds_free(cols);
    sds_free(y_hw);

    free(y_sw);
    printf("\rBye mm!\n\r");

    return status;
}
