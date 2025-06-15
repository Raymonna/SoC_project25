#ifndef __SPMV_H__
#define __SPMV_H__

#include "ap_int.h"

#define  ROWS          16
//4000
#define  COLS          16
//4000
#define  DATA_LENGTH   16
//20000

#define  MAX_COL_SIZE  16
//9800
#define  MAX_ROW_SIZE  16//
//9800


//typedef ap_uint<32> u32;
typedef unsigned int u32;
typedef float DATA_TYPE;




int spmv_accel(
		DATA_TYPE       values[DATA_LENGTH],
		u32        		cols[DATA_LENGTH],
		u32        		rows[ROWS],
		DATA_TYPE  		x[COLS],
		DATA_TYPE  		y[ROWS],

		u32        		 row_size,
		u32        		 col_size,
		u32        		 data_size
	);

#endif //__SPMV_H__
