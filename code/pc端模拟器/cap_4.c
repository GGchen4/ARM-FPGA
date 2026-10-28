#include <stdio.h>
#include <stdlib.h>
#include <math.h>

// 计算二维数组的线性索引
#define IDX2C(i, j, ld) (((i) * (ld)) + (j))

// 矩阵乘法 A (m x k) * B (k x n) = C (m x n)
void matmul(float* A, int A_rows, int A_cols, float* B, int B_cols, float* C) {
    for (int i = 0; i < A_rows; i++) {
        for (int j = 0; j < B_cols; j++) {
            C[IDX2C(i, j, B_cols)] = 0;
            for (int k = 0; k < A_cols; k++) {
                C[IDX2C(i, j, B_cols)] += A[IDX2C(i, k, A_cols)] * B[IDX2C(k, j, B_cols)];
            }
        }
    }
}

// 矩阵转置
void transpose(float* A, int rows, int cols, float* T) {
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            T[IDX2C(j, i, cols)] = A[IDX2C(i, j, rows)];
        }
    }
}

// squash 函数
void squash(float* s, int rows, int cols) {
    for (int i = 0; i < rows; i++) {
        float norm = 0;
        for (int j = 0; j < cols; j++) {
            norm += s[IDX2C(i, j, cols)] * s[IDX2C(i, j, cols)];
        }
        norm = sqrtf(norm);
        float scale = norm / (1 + norm);
        for (int j = 0; j < cols; j++) {
            s[IDX2C(i, j, cols)] = scale * s[IDX2C(i, j, cols)] / (norm + 1e-8);
        }
    }
}

// 初始化 CapsuleLayer1d
void CapsuleLayer1d_init(int num_out_capsule, int num_in_capsule, int out_capsule_dim, int in_capsule_dim, float* affine_w) {
    // 简单的权重初始化为随机值
    for (int i = 0; i < num_out_capsule; i++) {
        for (int j = 0; j < num_in_capsule; j++) {
            for (int k = 0; k < out_capsule_dim; k++) {
                for (int l = 0; l < in_capsule_dim; l++) {
                    affine_w[IDX2C(i * out_capsule_dim + k, j * in_capsule_dim + l, num_out_capsule * out_capsule_dim)] = (float)rand() / RAND_MAX;
                }
            }
        }
    }
}

// 前向传播
void CapsuleLayer1d_forward(int num_in_capsule, int in_capsule_dim, int num_out_capsule, int out_capsule_dim, int routings, float* x, float* affine_w, float* output) {
    int u_hat_rows = num_out_capsule * num_in_capsule;
    float* u_hat = (float*)malloc(u_hat_rows * out_capsule_dim * sizeof(float));
    float* x_expanded = (float*)malloc(num_in_capsule * in_capsule_dim * sizeof(float));
    float* b = (float*)calloc(num_out_capsule * num_in_capsule, sizeof(float));

    // 扩展 x 的维度
    for (int i = 0; i < num_in_capsule * in_capsule_dim; i++) {
        x_expanded[i] = x[i];
    }

    // 矩阵乘法 u_hat = x_expanded * affine_w^T
    matmul(x_expanded, num_in_capsule, in_capsule_dim, affine_w, out_capsule_dim * num_out_capsule, u_hat);

    for (int r = 0; r < routings; r++) {
        // Softmax 简化版
        for (int i = 0; i < num_out_capsule; i++) {
            float sum = 0;
            for (int j = 0; j < num_in_capsule; j++) {
                b[IDX2C(i, j, num_out_capsule)] = exp(b[IDX2C(i, j, num_out_capsule)]);
                sum += b[IDX2C(i, j, num_out_capsule)];
            }
            for (int j = 0; j < num_in_capsule; j++) {
                b[IDX2C(i, j, num_out_capsule)] /= sum;
            }
        }

        // 矩阵乘法 output = b * u_hat
        matmul(b, num_out_capsule, num_in_capsule, u_hat, out_capsule_dim, output);
        squash(output, num_out_capsule, out_capsule_dim);

        if (r < routings - 1) {
            float* delta_b = (float*)malloc(num_out_capsule * num_in_capsule * sizeof(float));
            float* output_T = (float*)malloc(out_capsule_dim * num_out_capsule * sizeof(float));
            transpose(output, num_out_capsule, out_capsule_dim, output_T);
            matmul(u_hat, num_out_capsule, out_capsule_dim, output_T, num_in_capsule, delta_b);
            for (int i = 0; i < num_out_capsule; i++) {
                for (int j = 0; j < num_in_capsule; j++) {
                    b[IDX2C(i, j, num_out_capsule)] += delta_b[IDX2C(i, j, num_out_capsule)];
                }
            }
            free(delta_b);
            free(output_T);
        }
    }

    free(u_hat);
    free(x_expanded);
    free(b);
}

// 测试函数
int main() {
    int num_in_capsule = 3;
    int in_capsule_dim = 64;
    int num_out_capsule = 3;
    int out_capsule_dim = 16;
    int routings = 3;

    float* affine_w = (float*)malloc(num_out_capsule * out_capsule_dim * num_in_capsule * in_capsule_dim * sizeof(float));
    CapsuleLayer1d_init(num_out_capsule, num_in_capsule, out_capsule_dim, in_capsule_dim, affine_w);

    float* x = (float*)malloc(num_in_capsule * in_capsule_dim * sizeof(float));
    for (int i = 0; i < num_in_capsule * in_capsule_dim; i++) {
        x[i] = (float)rand() / RAND_MAX;
    }

    float* output = (float*)malloc(num_out_capsule * out_capsule_dim * sizeof(float));
    CapsuleLayer1d_forward(num_in_capsule, in_capsule_dim, num_out_capsule, out_capsule_dim, routings, x, affine_w, output);

    // 打印输出
    for (int i = 0; i < num_out_capsule; i++) {
        for (int j = 0; j < out_capsule_dim; j++) {
            printf("%f ", output[IDX2C(i, j, num_out_capsule)]);
        }
        printf("\n");
    }

    free(x);
    free(output);
    free(affine_w);

    return 0;
}
