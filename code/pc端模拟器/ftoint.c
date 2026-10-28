#include <stdio.h>
#include <math.h>

// 将小数转换为Q格式定点数并以整数形式返回
unsigned int float_to_fixed(float x, int m, int n) {
    // 乘以2的n次方
    float scaled = x * (1 << n);
    
    // 四舍五入
    unsigned int fixed_point = round(scaled);
    
    // 返回结果
    return fixed_point;
}
float fixed_to_float( int fixed_point, int m, int n) {
    return (float)fixed_point / (1 << n);
}

// 将整数转换为二进制并打印
void print_binary(unsigned int num, int total_bits) {
    for (int i = total_bits - 1; i >= 0; i--) {
        unsigned int bit = (num >> i) & 1;
        printf("%d", bit);
    }
    printf("\n");
}

int main() {
   //38.678 12.489   0.01916750307333021867638483965015
    float x =-0.216221406826812;
    int m = 1;
    int n = 15;

    // 转换为Q格式定点数
    unsigned int fixed_point = float_to_fixed(x, m, n);

    // 定点数转换为浮点数
    int y =3307;
    float float_point = fixed_to_float(y,m,15);
    // 计算总位数
    int total_bits = m + n;

    // 打印结果
    printf("Fixed-point representation (integer): %d\n", fixed_point);
    printf("Fixed-point representation (binary): ");
    print_binary(fixed_point, total_bits);

    printf("Float-point representation (float): %f\n", float_point);
    return 0;
}
