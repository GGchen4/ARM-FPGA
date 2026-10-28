#include <stdio.h>
#include <stdlib.h>
#include <math.h>

// 展平数组的索引计算函数
#define IDX4D(i, j, k, l, dim2, dim3, dim4) (((i) * (dim2) * (dim3) * (dim4)) + ((j) * (dim3) * (dim4)) + ((k) * (dim4)) + (l))
#define IDX3D(i, j, k, dim2, dim3) (((i) * (dim2) * (dim3)) + ((j) * (dim3)) + (k))

float* create_flat_array(int size) {
    return (float*)malloc(size * sizeof(float));
}

void matrix_multiply(float* affine_w_flat, float* x_flat, float* u_hat_flat, 
                     int num_out_capsule, int num_in_capsule, 
                     int out_capsule_dim, int in_capsule_dim, int batch_size) {
//num_out_capsule = 3 num_in_capsule = 7 * 3 out_capsule_dim = 16 in_capsule_dim = 64 x_flat[1,21,64]  affine_w_flat[3,21,16,64]
    for (int b = 0; b < batch_size; b++) {
        for (int i = 0; i < num_out_capsule; i++) {
            for (int j = 0; j < num_in_capsule; j++) {
                for (int k = 0; k < out_capsule_dim; k++) {
                    float sum = 0;
                    for (int l = 0; l < in_capsule_dim; l++) {
                        // 对应矩阵乘法的元素计算
                        sum += x_flat[IDX3D(b, j, l, num_in_capsule, in_capsule_dim)] * 
                               affine_w_flat[IDX4D(i, j, l, k, num_in_capsule, in_capsule_dim, out_capsule_dim)];
                    }
                    u_hat_flat[IDX3D(b, i, j, num_out_capsule, num_in_capsule)] = sum;
                }
            }
        }
    }
}

int main() {
    int num_out_capsule = 3, num_in_capsule = 21, out_capsule_dim = 16, in_capsule_dim = 64, batch_size = 1;

    // 展平后的数组大小
    int affine_w_size = num_out_capsule * num_in_capsule * out_capsule_dim * in_capsule_dim;
    int x_size = batch_size * num_in_capsule * in_capsule_dim;
    int u_hat_size = batch_size * num_out_capsule * num_in_capsule * out_capsule_dim;

    // 动态分配展平的数组
    float* affine_w_flat = create_flat_array(affine_w_size);
    float* x_flat = create_flat_array(x_size);
    float* u_hat_flat = create_flat_array(u_hat_size);

    // 初始化 affine_w_flat 和 x_flat （这里省略初始化步骤）

    // 进行矩阵乘法
    matrix_multiply(affine_w_flat, x_flat, u_hat_flat, num_out_capsule, num_in_capsule, out_capsule_dim, in_capsule_dim, batch_size);

    // 打印 u_hat_flat 的部分结果
    for (int i = 0; i < u_hat_size; i++) {
        printf("%f ", u_hat_flat[i]);
    }
    printf("\n");

    // 释放内存
    free(affine_w_flat);
    free(x_flat);
    free(u_hat_flat);

    return 0;
}
