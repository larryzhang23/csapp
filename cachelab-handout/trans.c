/* 
 * trans.c - Matrix transpose B = A^T
 *
 * Each transpose function must have a prototype of the form:
 * void trans(int M, int N, int A[N][M], int B[M][N]);
 *
 * A transpose function is evaluated by counting the number of misses
 * on a 1KB direct mapped cache with a block size of 32 bytes.
 */ 
#include <stdio.h>
#include "cachelab.h"

int is_transpose(int M, int N, int A[N][M], int B[M][N]);

/* 
 * transpose_submit - This is the solution transpose function that you
 *     will be graded on for Part B of the assignment. Do not change
 *     the description string "Transpose submission", as the driver
 *     searches for that string to identify the transpose function to
 *     be graded. 
 */
char transpose_submit_desc[] = "Transpose submission";
void transpose_submit(int M, int N, int A[N][M], int B[M][N])
{       
    if (M == 32 && N == 32) {
       /* Divide the matrix to be 8x8 blocks to process because each cache line/block can contain at most 8 data points */
       int i, j, k;
       /* The 8 registers are essential to get the full score because they help eliminate the conflict misses along the diagonal */
       int tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7, tmp8;
       for (i = 0; i < 32; i += 8) 
            for (j = 0; j < 32; j += 8) 
                for (k = i; k < i + 8; k++) {
                    tmp1 = A[k][j];
                    tmp2 = A[k][j + 1];
                    tmp3 = A[k][j + 2];
                    tmp4 = A[k][j + 3];
                    tmp5 = A[k][j + 4];
                    tmp6 = A[k][j + 5];
                    tmp7 = A[k][j + 6];
                    tmp8 = A[k][j + 7];

                    B[j][k] = tmp1;
                    B[j + 1][k] = tmp2;
                    B[j + 2][k] = tmp3;
                    B[j + 3][k] = tmp4;
                    B[j + 4][k] = tmp5;
                    B[j + 5][k] = tmp6;
                    B[j + 6][k] = tmp7;
                    B[j + 7][k] = tmp8;
                }
    } else if (M == 64 && N == 64) {
        int i, j, k;
        int tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7, tmp8;
        
        for (i = 0; i < N; i += 8) {
            for (j = 0; j < M; j += 8) {
                /* Move first 4 rows of A to the first 4 rows of B */
                for (k = i; k < i + 4; k++) {
                    tmp1 = A[k][j];
                    tmp2 = A[k][j + 1];
                    tmp3 = A[k][j + 2];
                    tmp4 = A[k][j + 3];
                    tmp5 = A[k][j + 4];
                    tmp6 = A[k][j + 5];
                    tmp7 = A[k][j + 6];
                    tmp8 = A[k][j + 7];

                    B[j][k] = tmp1;
                    B[j + 1][k] = tmp2;
                    B[j + 2][k] = tmp3;
                    B[j + 3][k] = tmp4;

                    B[j][k + 4] = tmp5;
                    B[j + 1][k + 4] = tmp6;
                    B[j + 2][k + 4] = tmp7;
                    B[j + 3][k + 4] = tmp8;
                }
                /* Move the top right 4x4 of B to the bottom left while moving bottom left 4x4 of A to top right of B.
                   In this case, the bottom 4 rows of A, bottom 4 rows of B, top 4 rows of B are conflict at the same time.
                   THUS,
                   Here we have to read A by row-first order. So that the value will be mapping to the top right of B along the row.
                   So that when we copy the values from top right B to the bottom right, it is also along the row.
                   This helps decrease the conflict misses. 
                */
                // Now use k as the column index 
                for (k = j; k < j + 4; k++) {
                    tmp1 = A[i + 4][k];
                    tmp2 = A[i + 5][k];
                    tmp3 = A[i + 6][k];
                    tmp4 = A[i + 7][k];

                    tmp5 = B[k][i + 4];
                    tmp6 = B[k][i + 5];
                    tmp7 = B[k][i + 6];
                    tmp8 = B[k][i + 7];
                    // update the row immediately  after loading op to avoid conflict miss
                    B[k][i + 4] = tmp1;
                    B[k][i + 5] = tmp2;
                    B[k][i + 6] = tmp3;
                    B[k][i + 7] = tmp4;

                    B[k + 4][i] = tmp5;
                    B[k + 4][i + 1] = tmp6;
                    B[k + 4][i + 2] = tmp7;
                    B[k + 4][i + 3] = tmp8;                    
                }
                /* Move the bottom right of A to the bottom right of B */
                for (k = i; k < i + 4; k++) {
                    tmp1 = A[k + 4][j + 4];
                    tmp2 = A[k + 4][j + 5];
                    tmp3 = A[k + 4][j + 6];
                    tmp4 = A[k + 4][j + 7];

                    B[j + 4][k + 4] = tmp1;
                    B[j + 5][k + 4] = tmp2;
                    B[j + 6][k + 4] = tmp3;
                    B[j + 7][k + 4] = tmp4;
                }
            }
        }   
    } else { 
        // No specific way, random block divide
        int i, j, x, y;
        int blksize = 16;
        for (i = 0; i < N; i += blksize) {      
            for (j = 0; j < M; j += blksize) {  
                for (x = i; x < i + blksize; x++) {
                    if (x >= N) 
                        continue;
                    for (y = j; y < j + blksize; y++) {
                        if (y >= M)
                            continue;
                        B[y][x] = A[x][y];
                    } 
                }
            }
        }
       
    }
        
}


/* 
 * You can define additional transpose functions below. We've defined
 * a simple one below to help you get started. 
 */ 
char transpose_simple_block_desc[] = "Transpose simple block solution";
void transpose_simple_block(int M, int N, int A[N][M], int B[M][N]) {
    if (M == 32 && N == 32) {
       /* Divide the matrix to be 8x8 blocks to process because each cache line/block can contain at most 8 data points */
       int i, j, k;
       /* The 8 registers are essential to get the full score because they help eliminate the conflict misses along the diagonal */
       int tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7, tmp8;
       for (i = 0; i < 32; i += 8) 
            for (j = 0; j < 32; j += 8) 
                for (k = i; k < i + 8; k++) {
                    tmp1 = A[k][j];
                    tmp2 = A[k][j + 1];
                    tmp3 = A[k][j + 2];
                    tmp4 = A[k][j + 3];
                    tmp5 = A[k][j + 4];
                    tmp6 = A[k][j + 5];
                    tmp7 = A[k][j + 6];
                    tmp8 = A[k][j + 7];

                    B[j][k] = tmp1;
                    B[j + 1][k] = tmp2;
                    B[j + 2][k] = tmp3;
                    B[j + 3][k] = tmp4;
                    B[j + 4][k] = tmp5;
                    B[j + 5][k] = tmp6;
                    B[j + 6][k] = tmp7;
                    B[j + 7][k] = tmp8;
                }
    } else if (M == 64 && N == 64) {
        int i, j, k;
        /* The 4 registers are essential to get the full score because they help eliminate the conflict misses along the diagonal */
        int tmp1, tmp2, tmp3, tmp4;
        for (i = 0; i < N; i += 4) 
            for (j = 0; j < M; j += 4) 
                for (k = i; k < i + 4; k++) {
                    if (k >= N) 
                        continue;
                    tmp1 = A[k][j];
                    if (j + 1 < M) tmp2 = A[k][j + 1];
                    if (j + 2 < M) tmp3 = A[k][j + 2];
                    if (j + 3 < M) tmp4 = A[k][j + 3];

                    B[j][k] = tmp1;
                    if (j + 1 < M) B[j + 1][k] = tmp2;
                    if (j + 1 < M) B[j + 2][k] = tmp3;
                    if (j + 1 < M) B[j + 3][k] = tmp4;
                }
    } else {
        /* No idea why this works but it is close to full points*/
        int ii, jj;
        int i, j;
        int ik, jk;
        // block size is still 8
        int bsize = 8;
        // inner block size is 4
        int ibsize = 4;
        // the order we map each block is by handling 4x4 blocks inside each 8x8 block
        for (ii = 0; ii < N; ii += bsize) {
            for (jj = 0; jj < M; jj += bsize) {
                // change the order to process the 4x4 blocks by row first
                for (jk = jj; jk < jj + bsize; jk += ibsize) {  
                    for (ik = ii; ik < ii + bsize; ik += ibsize) {
                        for (i = ik; i < ik + ibsize; i++) {
                            if (i >= N)
                                continue;
                            for (j = jk; j < jk + ibsize; j++) {
                                if (j >= M)
                                    continue;
                                B[j][i] = A[i][j];
                            }
                                
                        }
                    }  
                }
            }
        }   
    }
}

/* 
 * trans - A simple baseline transpose function, not optimized for the cache.
 */
char trans_desc[] = "Simple row-wise scan transpose";
void trans(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, tmp;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; j++) {
            tmp = A[i][j];
            B[j][i] = tmp;
        }
    }    

}

/*
 * registerFunctions - This function registers your transpose
 *     functions with the driver.  At runtime, the driver will
 *     evaluate each of the registered functions and summarize their
 *     performance. This is a handy way to experiment with different
 *     transpose strategies.
 */
void registerFunctions()
{
    /* Register your solution function */
    registerTransFunction(transpose_submit, transpose_submit_desc); 

    /* Register any additional transpose functions */
    registerTransFunction(transpose_simple_block, transpose_simple_block_desc);
    // registerTransFunction(trans, trans_desc); 

}

/* 
 * is_transpose - This helper function checks if B is the transpose of
 *     A. You can check the correctness of your transpose by calling
 *     it before returning from the transpose function.
 */
int is_transpose(int M, int N, int A[N][M], int B[M][N])
{
    int i, j;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; ++j) {
            if (A[i][j] != B[j][i]) {
                return 0;
            }
        }
    }
    return 1;
}

