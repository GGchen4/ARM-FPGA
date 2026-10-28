#include <stdio.h>
#include <stdlib.h>
#include <math.h>

// 定义矩阵结构体
typedef struct {
    int rows;
    int cols;
    float **data;
} Matrix;

// 动态分配矩阵内存
Matrix* create_matrix(int rows, int cols) {
    Matrix* mat = (Matrix*)malloc(sizeof(Matrix));
    mat->rows = rows;
    mat->cols = cols;
    mat->data = (float**)malloc(rows * sizeof(float*));
    for (int i = 0; i < rows; i++) {
        mat->data[i] = (float*)malloc(cols * sizeof(float));
    }
    return mat;
}

// 释放矩阵内存
void free_matrix(Matrix* mat) {
    for (int i = 0; i < mat->rows; i++) {
        free(mat->data[i]);
    }
    free(mat->data);
    free(mat);
}

// 矩阵乘法
Matrix* matmul(Matrix* A, Matrix* B) {
    if (A->cols != B->rows) {
        printf("矩阵维度不匹配\n");
        exit(1);
    }
    Matrix* C = create_matrix(A->rows, B->cols);
    for (int i = 0; i < A->rows; i++) {
        for (int j = 0; j < B->cols; j++) {
            C->data[i][j] = 0;
            for (int k = 0; k < A->cols; k++) {
                C->data[i][j] += A->data[i][k] * B->data[k][j];
            }
        }
    }
    return C;
}

// 矩阵转置
Matrix* transpose(Matrix* A) {
    Matrix* T = create_matrix(A->cols, A->rows);
    for (int i = 0; i < A->rows; i++) {
        for (int j = 0; j < A->cols; j++) {
            T->data[j][i] = A->data[i][j];
        }
    }
    return T;
}

// squash 函数
void squash(Matrix* s) {
    for (int i = 0; i < s->rows; i++) {
        float norm = 0;
        for (int j = 0; j < s->cols; j++) {
            norm += s->data[i][j] * s->data[i][j];
        }
        norm = sqrtf(norm);
        float scale = norm / (1 + norm);
        for (int j = 0; j < s->cols; j++) {
            s->data[i][j] = scale * s->data[i][j] / (norm + 1e-8);
        }
    }
}

// CapsuleLayer1d 结构体
typedef struct {
    int num_in_capsule;
    int in_capsule_dim;
    int num_out_capsule;
    int out_capsule_dim;
    int routings;
    Matrix* affine_w;
} CapsuleLayer1d;

// 初始化 CapsuleLayer1d
CapsuleLayer1d* CapsuleLayer1d_init(int num_in_capsule, int in_capsule_dim, int num_out_capsule, int out_capsule_dim, int routings) {
    CapsuleLayer1d* layer = (CapsuleLayer1d*)malloc(sizeof(CapsuleLayer1d));
    layer->num_in_capsule = num_in_capsule;
    layer->in_capsule_dim = in_capsule_dim;
    layer->num_out_capsule = num_out_capsule;
    layer->out_capsule_dim = out_capsule_dim;
    layer->routings = routings;

    layer->affine_w = create_matrix(num_out_capsule * out_capsule_dim, num_in_capsule * in_capsule_dim);
    // 简单的权重初始化为随机值
    for (int i = 0; i < layer->affine_w->rows; i++) {
        for (int j = 0; j < layer->affine_w->cols; j++) {
            layer->affine_w->data[i][j] = (float)rand() / RAND_MAX;
        }
    }

    return layer;
}

// 前向传播
Matrix* CapsuleLayer1d_forward(CapsuleLayer1d* layer, Matrix* x) {
    // 扩展 x 的维度
    Matrix* u_hat = matmul(x, transpose(layer->affine_w));
    
    // 初始化 b 为全零矩阵
    Matrix* b = create_matrix(layer->num_out_capsule, layer->num_in_capsule);

    // 动态路由
    for (int i = 0; i < layer->routings; i++) {
        // 这里模拟 softmax 操作
        for (int j = 0; j < b->rows; j++) {
            float sum = 0;
            for (int k = 0; k < b->cols; k++) {
                b->data[j][k] = exp(b->data[j][k]);
                sum += b->data[j][k];
            }
            for (int k = 0; k < b->cols; k++) {
                b->data[j][k] /= sum;
            }
        }

        Matrix* outputs = matmul(b, u_hat);
        squash(outputs);

        if (i < layer->routings - 1) {
            Matrix* delta_b = matmul(u_hat, transpose(outputs));
            for (int j = 0; j < b->rows; j++) {
                for (int k = 0; k < b->cols; k++) {
                    b->data[j][k] += delta_b->data[j][k];
                }
            }
            free_matrix(delta_b);
        }

        free_matrix(outputs);
    }

    // 最终输出
    Matrix* outputs = matmul(b, u_hat);
    free_matrix(b);
    return outputs;
}

// 清理 CapsuleLayer1d
void CapsuleLayer1d_free(CapsuleLayer1d* layer) {
    free_matrix(layer->affine_w);
    free(layer);
}

// 测试函数
int main() {
    CapsuleLayer1d* layer = CapsuleLayer1d_init(3, 64, 3, 16, 3);

    Matrix* x = create_matrix(1, 64); // 输入维度假设为1x64
    for (int i = 0; i < x->rows; i++) {
        for (int j = 0; j < x->cols; j++) {
            x->data[i][j] = (float)rand() / RAND_MAX;
        }
    }

    Matrix* output = CapsuleLayer1d_forward(layer, x);

    // 打印输出
    for (int i = 0; i < output->rows; i++) {
        for (int j = 0; j < output->cols; j++) {
            printf("%f ", output->data[i][j]);
        }
        printf("\n");
    }

    free_matrix(x);
    free_matrix(output);
    CapsuleLayer1d_free(layer);

    return 0;
}
