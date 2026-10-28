#ifndef __CONV_CORE_H__
#define __CONV_CORE_H__

#include <ap_int.h>
#include <iostream>
#include <ap_fixed.h>

using namespace std;

#define cut 1
// 单个数据是16bit位宽，一个子块数据是16*K位宽，用bus表示。
// 输入图层和权重都是ap_int<16>
//typedef ap_fixed<16, 1> fix16_t;
//typedef ap_fixed<32, 2> fix16_mul;
//typedef ap_fixed<40, 10> fix16_acc;

typedef ap_fixed<8, 1> fix16_t;
typedef ap_fixed<16, 2> fix16_mul;
typedef ap_fixed<20, 6> fix16_acc;

//typedef ap_fixed<4, 1> fix16_t;
//typedef ap_fixed<8, 2> fix16_mul;
//typedef ap_fixed<10, 4> fix16_acc;

typedef ap_int<16>   dtype_dat;
typedef ap_int<16*cut> dtype_bus;
// 两个INT16数据相乘，得到的结果会溢出，用32位来存。根据经验，用40位来作为累加的结果位宽。
typedef ap_int<32>   dtype_mul;
typedef ap_int<32*cut> dtype_mul_bus;
typedef ap_int<40>   dtype_acc;

//#define Tn 4  //并行的输入通道数
//#define Tm 4  //并行输出通道数
//#define K 3   //卷积核尺寸
//#define Tr 6  //并行处理得到输出特征图的个数，这里是1
//#define Tc 10  //并行处理得到输出特征图的个数，这里是1
const int Tn = 4 ; //并行的输入通道数
const int Tm =4 ; //并行输出通道数
const int K =3  ; //卷积核尺寸
const int Tr =6 ; //并行处理得到输出特征图的个数，这里是1
const int Tc =10 ; //并行处理得到输出特征图的个数，这里是1
// C语言无法传可变索引的数组的，但在硬件内存中，都按照一维分布，故可以把地址传进去。
// 后续得自己计算存放的地址来取数据
// 定点化后的数据由三个小数点的参数来计算得到，INT16，小数点由四位来表示
void load_weight_tile(
	fix16_t* weight,
	fix16_t  weight_buf[Tm*Tn*K*K],
    int m_offset, int n_offset,
    int M, int N
);
void load_input_tile(
		fix16_t* input,
		fix16_t input_tile_buf[Tn*(Tr+K-1)*(Tc+K-1)],
    int n_offset, int h_offset, int w_offset,
    int N, int H, int W
) ;
void Conv(
	ap_uint<16> CHin,  // 输入通道数（输入图层方向上的数目）（卷积核与特征图参量）
	ap_uint<16> Hin,   // 输入特征的高度（卷积核与特征图参量）
	ap_uint<16> Win,   // 输入特征的宽度（卷积核与特征图参量）
	ap_uint<16> CHout, // 输出特征的通道数（卷积核参量）
	ap_uint<8> Kx,     // 卷积核的宽度
	ap_uint<8> Ky,     // 卷积核的高度
	ap_uint<8> Sx,     // 卷积核扫描时，宽度方向的步进
	ap_uint<8> Sy,     // 卷积核扫描时，高度方向的步进
	ap_uint<1> mode,   // 卷积的模式，valid还是same
	ap_uint<1> relu_en,                // 激活非线性反激活层标志
	fix16_t *Feature_in_dache,            // 输入feature map的数据，由于C中无法使用可变数组，故使用指针，传地址来定位具体数据，四维数组按一维分布
	// 之前池化使用的是固定大小的，可以按照固定数组来传，但这里数组长度是变的
	fix16_t *W_dache,                     // 输入权重的数据，也是指针索引
	fix16_t *Feature_out_dache           // 输出feature map的数据，也是指针索引
	);
//void top(dtype_bus* Feature_in_dache,dtype_bus* Feature_out_dache);
//void Conv1(
//	dtype_bus Feature_in_dache[2160],            // 输入feature map的数据，由于C中无法使用可变数组，故使用指针，传地址来定位具体数据，四维数组按一维分布
//	// 之前池化使用的是固定大小的，可以按照固定数组来传，但这里数组长度是变的
//	dtype_bus Feature_out_dache[34560]           // 输出feature map的数据，也是指针索引
//	);
//void Conv2(
//	dtype_bus Feature_in_dache[34560],            // 输入feature map的数据，由于C中无法使用可变数组，故使用指针，传地址来定位具体数据，四维数组按一维分布
//	dtype_bus Feature_out_dache[34560]           // 输出feature map的数据，也是指针索引
//	);
//void Conv3(
//	dtype_bus Feature_in_dache[34560],            // 输入feature map的数据，由于C中无法使用可变数组，故使用指针，传地址来定位具体数据，四维数组按一维分布
//	dtype_bus Feature_out_dache[34560]           // 输出feature map的数据，也是指针索引
//	);
#endif
