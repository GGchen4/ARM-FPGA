#include <stdio.h>
#include <stdlib.h>
#include <math.h>

// 创建二维数组
float** create_matrix(int rows, int cols) {
    float** mat = (float**)malloc(rows * sizeof(float*));
    for (int i = 0; i < rows; i++) {
        mat[i] = (float*)malloc(cols * sizeof(float));
    }
    return mat;
}

// 释放二维数组
void free_matrix(float** mat, int rows) {
    for (int i = 0; i < rows; i++) {
        free(mat[i]);
    }
    free(mat);
}

// 矩阵乘法
void matmul(float** A, int A_rows, int A_cols, float** B, int B_rows, int B_cols, float** C) {
    if (A_cols != B_rows) {
        printf("矩阵维度不匹配\n");
        exit(1);
    }
    for (int i = 0; i < A_rows; i++) {
        for (int j = 0; j < B_cols; j++) {
            C[i][j] = 0;
            for (int k = 0; k < A_cols; k++) {
                C[i][j] += A[i][k] * B[k][j];
            }
        }
    }
}

// 矩阵转置
void transpose(float** A, int rows, int cols, float** T) {
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            T[j][i] = A[i][j];
        }
    }
}

// squash 函数
void squash(float** s, int rows, int cols) {
    for (int i = 0; i < rows; i++) {
        float norm = 0;
        for (int j = 0; j < cols; j++) {
            norm += s[i][j] * s[i][j];
        }
        norm = sqrtf(norm);
        float scale = norm / (1 + norm);
        for (int j = 0; j < cols; j++) {
            s[i][j] = scale * s[i][j] / (norm + 1e-8);
        }
    }
}

// 初始化 CapsuleLayer1d
//num_in_capsule：输入胶囊的数量。in_capsule_dim：每个输入胶囊的维度。num_out_capsule：输出胶囊的数量。out_capsule_dim：每个输出胶囊的维度。routings：动态路由的迭代次数，默认值为 3。
void CapsuleLayer1d_init(int num_out_capsule, int num_in_capsule, int out_capsule_dim, int in_capsule_dim, float** affine_w) {
    // 简单的权重初始化为随机值
    for (int i = 0; i < num_out_capsule * out_capsule_dim; i++) {
        for (int j = 0; j < num_in_capsule * in_capsule_dim; j++) {
            affine_w[i][j] = (float)rand() / RAND_MAX;
        }
    }
}

// 前向传播
void CapsuleLayer1d_forward(int num_in_capsule, int in_capsule_dim, int num_out_capsule, int out_capsule_dim, int routings, float** x, float** affine_w, float** output) {
    float** u_hat = create_matrix(num_out_capsule, out_capsule_dim);
    float** x_T = create_matrix(in_capsule_dim, num_in_capsule);
    float** b = create_matrix(num_out_capsule, num_in_capsule);

    transpose(x, num_in_capsule, in_capsule_dim, x_T);
    matmul(affine_w, num_out_capsule * out_capsule_dim, num_in_capsule * in_capsule_dim, x_T, in_capsule_dim, num_in_capsule, u_hat);

    for (int r = 0; r < routings; r++) {
        // Softmax 简化版
        for (int i = 0; i < num_out_capsule; i++) {
            float sum = 0;
            for (int j = 0; j < num_in_capsule; j++) {
                b[i][j] = exp(b[i][j]);
                sum += b[i][j];
            }
            for (int j = 0; j < num_in_capsule; j++) {
                b[i][j] /= sum;
            }
        }

        matmul(b, num_out_capsule, num_in_capsule, u_hat, num_in_capsule, out_capsule_dim, output);
        squash(output, num_out_capsule, out_capsule_dim);

        if (r < routings - 1) {
            float** delta_b = create_matrix(num_out_capsule, num_in_capsule);
            float** output_T = create_matrix(out_capsule_dim, num_out_capsule);
            transpose(output, num_out_capsule, out_capsule_dim, output_T);
            matmul(u_hat, num_out_capsule, out_capsule_dim, output_T, out_capsule_dim, num_in_capsule, delta_b);
            for (int i = 0; i < num_out_capsule; i++) {
                for (int j = 0; j < num_in_capsule; j++) {
                    b[i][j] += delta_b[i][j];
                }
            }
            free_matrix(delta_b, num_out_capsule);
            free_matrix(output_T, out_capsule_dim);
        }
    }

    free_matrix(u_hat, num_out_capsule);
    free_matrix(x_T, in_capsule_dim);
    free_matrix(b, num_out_capsule);
}

// 测试函数
int main() {
    int num_in_capsule = 3;
    int in_capsule_dim = 64;
    int num_out_capsule = 3;
    int out_capsule_dim = 16;
    int routings = 3;

    float** affine_w = create_matrix(num_out_capsule * out_capsule_dim, num_in_capsule * in_capsule_dim);
    CapsuleLayer1d_init(num_out_capsule, num_in_capsule, out_capsule_dim, in_capsule_dim, affine_w);

    float** x = create_matrix(num_in_capsule, in_capsule_dim);
    for (int i = 0; i < num_in_capsule; i++) {
        for (int j = 0; j < in_capsule_dim; j++) {
            x[i][j] = (float)rand() / RAND_MAX;
        }
    }

    float** output = create_matrix(num_out_capsule, out_capsule_dim);
    CapsuleLayer1d_forward(num_in_capsule, in_capsule_dim, num_out_capsule, out_capsule_dim, routings, x, affine_w, output);

    // // 打印输出
    // for (int i = 0; i < num_out_capsule; i++) {
    //     for (int j = 0; j < out_capsule_dim; j++) {
    //         printf("%f ", output[i][j]);
    //     }
    //     printf("\n");
    // }

    // free_matrix(x, num_in_capsule);
    // free_matrix(output, num_out_capsule);
    // free_matrix(affine_w, num_out_capsule * out_capsule_dim);

    return 0;
}
