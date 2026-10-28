#include "conv_core.h"
const int TRIP_IN  = Tn * (Tr + K - 1) * (Tc + K - 1);
const int TRIP_W   = Tm * Tn * K * K;
const int TRIP_OUT = Tm * Tr * Tc;
//加载输入特征??

void load_input_tile(
		fix16_t* input,
		fix16_t input_tile_buf[Tn*(Tr+K-1)*(Tc+K-1)],
    int n_offset, int h_offset, int w_offset,
    int N, int H, int W
) {
#pragma HLS INLINE off
    for (int tn = 0; tn < Tn; tn++) {
        for (int i = 0; i < Tr + K-1; i++) {
            for (int j = 0; j < Tc + K-1; j++) {
//#pragma HLS UNROLL
#pragma HLS PIPELINE II=1
                int in_n = n_offset + tn;
                int in_h = h_offset + i;
                int in_w = w_offset + j;
                if (in_n < N && in_h < H && in_w < W)
//                    input_tile_buf[tn][i][j] = input[in_n * H * W + in_h * W + in_w];
                	input_tile_buf[tn*(Tr+K-1)*(Tc+K-1)+i*(Tc+K-1)+j] = input[in_n * H * W + in_h * W + in_w];
                else
//                    input_tile_buf[tn][i][j] = 0; // zero padding
                	input_tile_buf[tn*(Tr+K-1)*(Tc+K-1)+i*(Tc+K-1)+j] = 0; // zero padding
            }
        }
    }
}

//加载权重

void load_weight_tile(
	fix16_t* weight,
	fix16_t  weight_buf[Tm*Tn*K*K],
    int m_offset, int n_offset,
    int M, int N
) {
#pragma HLS INLINE off
    for (int tm = 0; tm < Tm; tm++) {
        for (int tn = 0; tn < Tn; tn++) {
            for (int i = 0; i < K; i++) {
                for (int j = 0; j < K; j++) {
//#pragma HLS UNROLL
#pragma HLS PIPELINE II=1
                    int w_m = m_offset + tm;
                    int w_n = n_offset + tn;
                    if (w_m < M && w_n < N)
//                        weight_buf[tm][tn][i][j] = weight[w_m * N * K * K + w_n * K * K + i * K + j];
                        weight_buf[tm*Tn*K*K+tn*K*K+i*K+j] = weight[w_m * N * K * K + w_n * K * K + i * K + j];
                    else
//                        weight_buf[tm][tn][i][j] = 0;
                    	weight_buf[tm*Tn*K*K+tn*K*K+i*K+j] = 0;
                }
            }
        }
    }
}

// 输入特征图四维：[C/K][H][W][K]
// 输入权重值五维：[CHout][Ky][Kx][CHin/K][K]
// 输出特征图四维：[C/K][H][W][K]，均可以计算出来
//#pragma HLS BIND_OP op=mul impl=dsp
void Conv(
	ap_uint<16> CHin,  // 输入通道数（输入图层方向上的数目）（卷积核与特征图参量）
	ap_uint<16> Hin,   // 输入特征的高度（卷积核与特征图参量）
	ap_uint<16> Win,   // 输入特征的宽度（卷积核与特征图参量）
	ap_uint<16> CHout, // 输出特征的???道数（卷积核参量）
	ap_uint<8> Kx,     // 卷积核的宽度
	ap_uint<8> Ky,     // 卷积核的高度
	ap_uint<8> Sx,     // 卷积核扫描时，宽度方向的步进
	ap_uint<8> Sy,     // 卷积核扫描时，高度方向的步进
	ap_uint<1> mode,   // 卷积的模式，valid还是same
	ap_uint<1> relu_en,                // ??活非线???反??活层标志
	fix16_t *Feature_in_dache,            // 输入feature map的数据，由于C中无法使用可变数组，故使用指针，传地??来定位具体数据，四维数组按一维分??
	// 之前池化使用的是固定大小的，可以按照固定数组来传，但这里数组长度是变??
	fix16_t *W_dache,                     // 输入权重的数据，也是指针索引
	fix16_t *Feature_out_dache           // 输出feature map的数据，也是指针索引
	)//mode: 0:VALID, 1:SAME
{
	#pragma HLS INTERFACE s_axilite port=return
	#pragma HLS INTERFACE s_axilite port=relu_en
	#pragma HLS INTERFACE s_axilite port=mode
	#pragma HLS INTERFACE s_axilite port=Sy
	#pragma HLS INTERFACE s_axilite port=Sx
	#pragma HLS INTERFACE s_axilite port=Ky
	#pragma HLS INTERFACE s_axilite port=Kx
	#pragma HLS INTERFACE s_axilite port=CHout
	#pragma HLS INTERFACE s_axilite port=Win
	#pragma HLS INTERFACE s_axilite port=Hin
	#pragma HLS INTERFACE s_axilite port=CHin

	#pragma HLS INTERFACE m_axi depth=999 port=Feature_out_dache offset=slave
	#pragma HLS INTERFACE m_axi depth=999 port=W_dache offset=slave
	#pragma HLS INTERFACE m_axi depth=999 port=Feature_in_dache offset=slave

//	fix16_t feature_in[34560];

	fix16_t feature_in[Tn*(Tr+K-1)*(Tc+K-1)];

	ap_uint<16> reg_1 = CHin*Hin*Win;
	fix16_t feature_in_bram[34560];

	LOOP_load1:for (int mm = 0; mm < Tn*(Tr+K-1)*(Tc+K-1); mm++) {
//#pragma HLS LOOP_TRIPCOUNT max=34560
#pragma HLS LOOP_TRIPCOUNT max=TRIP_IN
#pragma HLS PIPELINE
				feature_in[mm] = Feature_in_dache[mm];
	}

//	memcpy(feature_in,(fix16_t*)Feature_in_dache,reg_1*sizeof(fix16_t));

//	fix16_t W[2304];
	fix16_t W[Tm*Tn*K*K];

	ap_uint<16> reg_2 = CHin*Kx*Ky*CHout;
	fix16_t W_bram[2304];

	LOOP_load2:for (int mm = 0; mm < Tm*Tn*K*K; mm++) {
//#pragma HLS LOOP_TRIPCOUNT max=2304
#pragma HLS LOOP_TRIPCOUNT max=TRIP_W
#pragma HLS PIPELINE
				W[mm] = W_dache[mm];
	}

//	memcpy(W,(fix16_t*)W_dache,reg_2*sizeof(fix16_t));



	ap_uint<8> pad_x,pad_y;  // padding数据，表示x和y方向待填补的行列??
	if(mode==0)  // valid下不进行padding
	{
		pad_x=0;pad_y=0;
	}
	else  // same模式下进行padding，具体padding的数据???，在之前概念部分补充了
	{
		pad_x=(Kx-1)/2;pad_y=(Ky-1)/2;
	}
	// 通过输入的参数数据，计算输出图层的高度和宽度，这个的公式也是固定??
	ap_uint<16> Hout,Wout;
	Wout=(Win+2*pad_x-Kx)/Sx+1;
	Hout=(Hin+2*pad_y-Ky)/Sy+1;

//	fix16_t feature_out[34560];
	fix16_t feature_out[Tm*Tr*Tc];
//#pragma HLS ARRAY_PARTITION dim=1 factor=16 type=block variable=feature_out
	ap_uint<16> reg_3 = CHout*Hout*Wout;

//	memcpy(feature_out,(fix16_t*)Feature_out_dache,reg_3*sizeof(fix16_t));


//	fix16_acc sum=0;
//	fix16_acc sum[CHout];
//	fix16_t out_tp=0;

	// 对输出feature map的数据进行计??
	// 关于是输出方向上??要多少重循环的定位：输出是一个四维的数据，需要前三重做迭代，第四重做处理
	// ??是输出图像的高度，二是输出图像的宽度，三是输入图层方向上切块了多少，四是具体索引到哪个子??
	LOOP_i:for(int i=0;i<Hout;i=i+Tr)
	{
#pragma HLS LOOP_TRIPCOUNT max=6
		// 输出高度和宽度方向上的循??
		LOOP_j:for(int j=0;j<Wout;j=j+Tc)
		{
#pragma HLS LOOP_TRIPCOUNT max=6
			// 输出通道方向上的循环，CHout表示有多少个kernel，是卷积核专门的??个参数，与输入特征图无关
			LOOP_cout:for(int cout=0;cout<CHout;cout=cout+Tm)
			{
#pragma HLS LOOP_TRIPCOUNT max=4
				  fix16_acc sum[Tm*Tr*Tc] = {0};  // 在cout块内部定义并初始??

				LOOP_cin:for(int cin=0;cin<CHin;cin=cin+Tn)
				{
#pragma HLS LOOP_TRIPCOUNT max=4
//					load_input_tile(feature_in_bram,feature_in,cin,i,j,CHin,Hout,Wout);
//					load_weight_tile(W_bram,W,cout,cin,CHout,CHin);
					LOOP_ii:for(int ii=0;ii<3;ii++)

					{
						LOOP_jj:for(int jj=0;jj<3;jj++)
						{
#pragma HLS PIPELINE
							LOOP_rr:for(int rr=0;rr<Tr;rr++)
							{
								LOOP_cc:for(int cc=0;cc<Tc;cc++)
								{

									LOOP_Tm:for(int mm=0;mm<Tm;mm++)
									{
#pragma HLS UNROLL
										LOOP_Tn:for(int nn=0;nn<Tn;nn++)
										{
#pragma HLS UNROLL
									// CHin_div_K输入方向上切块的循环
									// 输入通道方向上被切块了，??要做??个循环，按输入???道上子块的数目

											ap_int<16> h=i*Sy-pad_y+ii;
											ap_int<16> w=j*Sx-pad_x+jj;

											fix16_mul tp;
											fix16_t dat;
											fix16_t wt;
											// 判断是否在padding的范围内
											// 这段if代码是获取特征图数据和权重数据的
											if(h>=0 && w>=0 && h<Hin && w<Win)
											{
												//dat = feature[cin+nn][h+rr][w+cc]

//												dat=feature_in[(cin+nn)*Hin*Win+(h+rr)*Win+(w+cc)];
//												wt=W[(cout+mm)*CHin*Kx*Ky+(cin+nn)*Kx*Ky+ii*Kx+jj];
//												tp = dat*wt;
//												sum[mm*Tr*Tc+rr*Tc+cc]+=tp;
												dat=feature_in[nn*(Tr+2)*(Tc+2)+rr*(Tc+2)+cc];
												wt=W[mm*Tn*Kx*Ky+nn*Kx*Ky+ii*Kx+jj];
												tp = dat*wt;
												sum[mm*Tr*Tc+rr*Tc+cc]+=tp;
											}
//											if((cin==CHin-1) && (jj==Kx-1) && (ii==Ky-1) )
//											{
//												// 如果使用了非线???激活层，小??0就设??0
//												if(sum[mm]<0)
//													sum[mm]=0;
//												fix16_t out_tp = sum[mm];
//												feature_out[(cout+mm)*Wout*Hout+i*Wout+j]=out_tp;out_tp=0;
//
//											}
										  }
									}

								}
							}

						}
					}
				}
				for (int mm = 0; mm < Tm; mm++) {
//#pragma HLS UNROLL factor=Tm
					for (int rr = 0; rr < Tr; rr++) {
//#pragma HLS UNROLL factor=Tr
						for (int cc = 0; cc < Tc; cc++) {
//#pragma HLS UNROLL factor=Tc
							if ((cout + mm) < CHout) {
								fix16_t out_val = sum[mm*Tr*Tc+rr*Tc+cc];
								if (relu_en && out_val < 0) out_val = 0;
//								feature_out[(cout+mm)*Wout*Hout + (i+rr)*Wout + (j+cc)] = out_val;
								feature_out[mm*Tr*Tc + rr*Tc + cc] = out_val;
							}
						}
					}
				}
			}
		}
	}
	//LOOP_load3:for (int mm = 0; mm < reg_3; mm++)
	LOOP_load3:for (int mm = 0; mm < Tm*Tr*Tc; mm++) {
#pragma HLS LOOP_TRIPCOUNT max=TRIP_OUT
//#pragma HLS LOOP_TRIPCOUNT max=34560//Tm*Tr*Tc
#pragma HLS PIPELINE
					Feature_out_dache[mm] = feature_out[mm];
	}
//	memcpy((fix16_t*)Feature_out_dache,feature_out,reg_3*sizeof(fix16_t));
}






//**********************************************测试*************************************************//

//void Conv1(
//	dtype_bus Feature_in_dache[2160],
//	dtype_bus Feature_out_dache[34560]
//	)//mode: 0:VALID, 1:SAME
//{
////	if(mode==0)  // valid下不进行padding
////	{
////		pad_x=0;pad_y=0;
////	}
////	else  // same模式下进行padding，具体padding的数据???，在之前概念部分补充了
////	{
////		pad_x=(Kx-1)/2;pad_y=(Ky-1)/2;
////	}
////	// 通过输入的参数数据，计算输出图层的高度和宽度，这个的公式也是固定??
////	ap_uint<16> Hout,Wout;
////	Wout=(Win+2*pad_x-Kx)/Sx+1;
////	Hout=(Hin+2*pad_y-Ky)/Sy+1;
//
//	dtype_bus W[144];
//	dtype_bus B[16];
////	float B[16]={
////	    	#include "./weight/conv1_bias.h"
////	};
//
//	dtype_acc sum=0;
//	dtype_bus out_tp=0;
//
//	// 对输出feature map的数据进行计??
//	// 关于是输出方向上??要多少重循环的定位：输出是一个四维的数据，需要前三重做迭代，第四重做处理
//	// ??是输出图像的高度，二是输出图像的宽度，三是输入图层方向上切块了多少，四是具体索引到哪个子??
//	LOOP_i:for(int i=0;i<36;i++)//Hout Wout=(Win+2*pad_x-Kx)/Sx+1;Hout=(Hin+2*pad_y-Ky)/Sy+1;根据这个公式计算，没有padding,pad_x就是0
//		{
//		// 输出高度和宽度方向上的循??
//		LOOP_j:for(int j=0;j<60;j++)//Wout
//			{
//
//			// 输出通道方向上的循环，CHout表示有多少个kernel，是卷积核专门的??个参数，与输入特征图无关
//			LOOP_cout:for(int cout=0;cout<16;cout=cout+1)//CHout 根据源代码来??
//					{
//
//				LOOP_cin:for(int cin=0;cin<1;cin=cin+1)//CHin_div_K 输入通道
//				{
//					#pragma HLS PIPELINE II=2
//				// 第四维上的数据处??
//				// 把卷积核size上一个点，在??有子块上的结果都求出来，再循环size上所有点
//					LOOP_ii:for(int ii=0;ii<3;ii++)//Ky 卷积核尺??
//						{
//						LOOP_jj:for(int jj=0;jj<3;jj++)//Kx 卷积核尺??
//							{
//
//						// CHin_div_K输入方向上切块的循环
//						// 输入通道方向上被切块了，??要做??个循环，按输入???道上子块的数目
//
//							ap_int<16> h=i*1-0+ii;//ap_int<16> h=i*Sy-pad_y+ii; 这里是判断是否padding的代码，有padding就是1，没有就??0
//							ap_int<16> w=j*1-0+jj;//ap_int<16> w=j*Sx-pad_x+jj; 同上，sy,sx是步长，根据python代码来写
//
//							dtype_mul_bus tp;
//							dtype_bus dat;
//							dtype_bus wt;
//							// 判断是否在padding的范围内
//							// 这段if代码是获取特征图数据和权重数据的
//							if(h>=0 && w>=0 && h<36 && w<60)//h>=0 && w>=0 && h<Hin && w<Win
//							{
//								// 输入数据dat=feature_in[cin][h][w][K]
//								// 根据四维数据的这个索引，来算出一维的数据
//								// INT16*K feature_in[C/K][H][W]为feature_in的形状，[K]相当于INT16*K??
//								// dat=feature_in[cin][h][w]为这个形状里取的索引
//
//								dat=Feature_in_dache[cin*36*60+h*60+w];//dat=Feature_in_dache[cin*Hin*Win+h*Win+w];
//
//								// INT16*K weight[CHout][Ky][Kx][CHin/K]为权重的形状，[K]相当于INT16*K??
//								// INT16*K weight[CHout][CHin/K][Ky][Kx][CHin/K]为权重的形状，[K]相当于INT16*K??
//								// wt=weight[cout][ii][jj][cin]为这个形状里取的索引
//
//								//wt=W[cout*CHin_div_K*Kx*Ky+ii*CHin_div_K*Kx+jj*CHin_div_K+cin];
//
//								wt=W[cout*1*3*3+cin*3*3+ii*3+jj];//wt=W[cout*CHin_div_K*Kx*Ky+cin*Kx*Ky+ii*Kx+jj];
//
//								//std::cout<<"dat="<<dat<<",W="<<wt<<std::endl;
//								tp.range(31,0)=(dtype_dat)dat.range(15,0)*(dtype_dat)wt.range(15,0);
//								sum+=(dtype_mul)tp.range(31,0);
//							}
//							else
//							{
//								dat=0;
//								wt=0;
//							}
//							//std::cout<<"sum="<<sum<<std::endl;
//							if((cin==1-1) && (jj==3-1) && (ii==3-1) )//CHin_div_K Kx Ky
//							{
//								// 如果使用了非线???激活层，小??0就设??0
//								if(1 & sum<0)//relu_en
//									sum=0;
//								// 数据右移
//								// 右移位数为输入feature map小数点位??+权重小数点位??-输出feature map小数点位??
//								dtype_acc res=sum>>15;//out_truncate
//
//								// 移位如果范围，进行前后的钳位
//								if(res>32767)
//									res=32767;
//								else
//									if(res<-32768)
//										res=-32768;
//
//								dtype_dat res_16=res;
////								out_tp.range((cout%1)*16+15,(cout%1)*16)=res;sum=0;
////								// feature_out[cout][i][j][cout]
////								if( ((cout%1)==1-1) || (cout==(CHout-1)) )
////								{
////									feature_out[(cout/1)*Wout*Hout+i*Wout+j]=out_tp;out_tp=0;
////								}
//								out_tp.range(15,0)=res + B[cout];sum=0;
//								Feature_out_dache[cout*36*60+i*60+j]=out_tp;out_tp=0;
//								//Feature_out_dache[cout*Wout*Hout+i*Wout+j]=out_tp;out_tp=0;
//							 }
//						   }
//					}
//				}
//			}
//		}
//	}
////	memcpy((dtype_bus*)Feature_out_dache,feature_out,reg_3*sizeof(dtype_bus));
//}


//************************************测试代码1************************************//
// 输入特征图四维：[C/K][H][W][K]
// 输入权重值五维：[CHout][Ky][Kx][CHin/K][K]
// 输出特征图四维：[C/K][H][W][K]，均可以计算出来

//void Conv2(
//	dtype_bus Feature_in_dache[34560],            // 输入feature map的数据，由于C中无法使用可变数组，故使用指针，传地??来定位具体数据，四维数组按一维分??
//	dtype_bus Feature_out_dache[34560]           // 输出feature map的数据，也是指针索引
//	)//mode: 0:VALID, 1:SAME
//{
////#pragma HLS ARRAY_PARTITION dim=1 factor=16 type=block variable=Feature_out_dache
////#pragma HLS ARRAY_PARTITION dim=1 factor=16 type=block variable=Feature_in_dache
//
////	dtype_bus feature_in[2160];
////	dtype_bus feature_in[65536];
////	ap_uint<16> reg_1 = CHin*Hin*Win;
////	memcpy(feature_in,(dtype_bus*)Feature_in_dache,reg_1*sizeof(dtype_bus));
//
//	dtype_bus W[2304];
//	dtype_bus B[27];
//#pragma HLS ARRAY_PARTITION dim=1 factor=16 type=block variable=W
////	ap_uint<16> reg_2 = CHin*Kx*Ky*CHout;
////	memcpy(W,(dtype_bus*)W_dache,reg_2*sizeof(dtype_bus));
//
//
////	ap_uint<5> out_truncate;
////	out_truncate=15;
//
//	// 通过输入的参数数据，计算输出图层的高度和宽度，这个的公式也是固定??
////	ap_uint<16> Hout,Wout;
////	Wout=(Win+2*pad_x-Kx)/Sx+1;
////	Hout=(Hin+2*pad_y-Ky)/Sy+1;
//
////	dtype_bus feature_out[131072];
////	dtype_bus feature_out[6480];
////	ap_uint<16> reg_3 = CHout*Hout*Wout;
////	memcpy(feature_out,(dtype_bus*)Feature_out_dache,reg_3*sizeof(dtype_bus));
//
//
////	dtype_acc sum=0;
////	dtype_bus out_tp=0;
//
//	// 对输出feature map的数据进行计??
//	// 关于是输出方向上??要多少重循环的定位：输出是一个四维的数据，需要前三重做迭代，第四重做处理
//	// ??是输出图像的高度，二是输出图像的宽度，三是输入图层方向上切块了多少，四是具体索引到哪个子??
//	LOOP_2i:for(int i=0;i<36;i++)
//	{
//		// 输出高度和宽度方向上的循??
//		LOOP_2j:for(int j=0;j<60;j++)
//		{
//
//			// 输出通道方向上的循环，CHout表示有多少个kernel，是卷积核专门的??个参数，与输入特征图无关
//			LOOP_2cout:for(int cout=0;cout<16;cout=cout+1)
//			{
//#pragma HLS PIPELINE II=2
//				// 第四维上的数据处??
//				// 把卷积核size上一个点，在??有子块上的结果都求出来，再循环size上所有点
//				LOOP_2ii:for(int ii=0;ii<3;ii++)
//				{
//
//					LOOP_2jj:for(int jj=0;jj<3;jj++)
//					{
//
//						// CHin_div_K输入方向上切块的循环
//						// 输入通道方向上被切块了，??要做??个循环，按输入???道上子块的数目
//						LOOP_2cin:for(int cin=0;cin<16;cin=cin+1)
//						{
//							dtype_acc sum=0;
//							dtype_bus out_tp=0;
//
//							ap_int<16> h=i*1-1+ii;
//							ap_int<16> w=j*1-1+jj;
//
//							dtype_mul_bus tp;
//							dtype_bus dat;
//							dtype_bus wt;
//							// 判断是否在padding的范围内
//							// 这段if代码是获取特征图数据和权重数据的
//							if(h>=0 && w>=0 && h<36 && w<60)
//							{
//								// 输入数据dat=feature_in[cin][h][w][K]
//								// 根据四维数据的这个索引，来算出一维的数据
//								// INT16*K feature_in[C/K][H][W]为feature_in的形状，[K]相当于INT16*K??
//								// dat=feature_in[cin][h][w]为这个形状里取的索引
//								dat=Feature_in_dache[cin*36*60+h*60+w];
//								// INT16*K weight[CHout][Ky][Kx][CHin/K]为权重的形状，[K]相当于INT16*K??
//								// INT16*K weight[CHout][CHin/K][Ky][Kx][CHin/K]为权重的形状，[K]相当于INT16*K??
//								// wt=weight[cout][ii][jj][cin]为这个形状里取的索引
//								wt=W[cout*1*3*3+cin*3*3+ii*3+jj];
//								tp.range(31,0)=(dtype_dat)dat.range(15,0)*(dtype_dat)wt.range(15,0);
//								sum+=(dtype_mul)tp.range(31,0);
//							}
//							else
//							{
//								dat=0;
//								wt=0;
//							}
//							// 每次取出??个特征图子块和权重子块，来进行累??
//							//std::cout<<"sum="<<sum<<std::endl;
//							if((cin==1-1) && (jj==3-1) && (ii==3-1) )
//							{
//								// 如果使用了非线???激活层，小??0就设??0
//								if(1 & sum<0)
//									sum=0;
//								// 数据右移
//								// 右移位数为输入feature map小数点位??+权重小数点位??-输出feature map小数点位??
//								dtype_acc res=sum>>15;
//
//								// 移位如果范围，进行前后的钳位
//								if(res>32767)
//									res=32767;
//								else
//									if(res<-32768)
//										res=-32768;
//
//								dtype_dat res_16=res;
//								out_tp.range(15,0)=res + B[cout];sum=0;
//								Feature_out_dache[cout*36*60+i*60+j]=out_tp;out_tp=0;
//							}
//						}
//					}
//				}
//			}
//		}
//	}
//}
//void Conv3(
//	dtype_bus Feature_in_dache[34560],
//	dtype_bus Feature_out_dache[34560]
//	)//mode: 0:VALID, 1:SAME
//{
////	if(mode==0)  // valid下不进行padding
////	{
////		pad_x=0;pad_y=0;
////	}
////	else  // same模式下进行padding，具体padding的数据???，在之前概念部分补充了
////	{
////		pad_x=(Kx-1)/2;pad_y=(Ky-1)/2;
////	}
////	// 通过输入的参数数据，计算输出图层的高度和宽度，这个的公式也是固定??
////	ap_uint<16> Hout,Wout;
////	Wout=(Win+2*pad_x-Kx)/Sx+1;
////	Hout=(Hin+2*pad_y-Ky)/Sy+1;
//
//	dtype_bus W[4608];
//	dtype_bus B[32];
////	float B[16]={
////	    	#include "./weight/conv1_bias.h"
////	};
//
//	dtype_acc sum=0;
//	dtype_bus out_tp=0;
//
//	// 对输出feature map的数据进行计??
//	// 关于是输出方向上??要多少重循环的定位：输出是一个四维的数据，需要前三重做迭代，第四重做处理
//	// ??是输出图像的高度，二是输出图像的宽度，三是输入图层方向上切块了多少，四是具体索引到哪个子??
//	LOOP_i:for(int i=0;i<36;i++)//Hout Wout=(Win+2*pad_x-Kx)/Sx+1;Hout=(Hin+2*pad_y-Ky)/Sy+1;根据这个公式计算，没有padding,pad_x就是0
//		{
//		// 输出高度和宽度方向上的循??
//		LOOP_j:for(int j=0;j<60;j++)//Wout
//			{
//
//			// 输出通道方向上的循环，CHout表示有多少个kernel，是卷积核专门的??个参数，与输入特征图无关
//			LOOP_cout:for(int cout=0;cout<16;cout=cout+1)//CHout 根据源代码来??
//					{
//
//				// 第四维上的数据处??
//				// 把卷积核size上一个点，在??有子块上的结果都求出来，再循环size上所有点
//				LOOP_ii:for(int ii=0;ii<3;ii++)//Ky 卷积核尺??
//					{
//
//					LOOP_jj:for(int jj=0;jj<3;jj++)//Kx 卷积核尺??
//						{
//							#pragma HLS PIPELINE II=4
//						// CHin_div_K输入方向上切块的循环
//						// 输入通道方向上被切块了，??要做??个循环，按输入???道上子块的数目
//						LOOP_cin:for(int cin=0;cin<16;cin=cin+1)//CHin_div_K 输入通道
//						{
//							ap_int<16> h=i*2-0+ii;//ap_int<16> h=i*Sy-pad_y+ii; 这里是判断是否padding的代码，有padding就是1，没有就??0
//							ap_int<16> w=j*2-0+jj;//ap_int<16> w=j*Sx-pad_x+jj; 同上，sy,sx是步长，根据python代码来写
//
//							dtype_mul_bus tp;
//							dtype_bus dat;
//							dtype_bus wt;
//							// 判断是否在padding的范围内
//							// 这段if代码是获取特征图数据和权重数据的
//							if(h>=0 && w>=0 && h<36 && w<60)//h>=0 && w>=0 && h<Hin && w<Win
//							{
//								// 输入数据dat=feature_in[cin][h][w][K]
//								// 根据四维数据的这个索引，来算出一维的数据
//								// INT16*K feature_in[C/K][H][W]为feature_in的形状，[K]相当于INT16*K??
//								// dat=feature_in[cin][h][w]为这个形状里取的索引
//
//								dat=Feature_in_dache[cin*36*60+h*60+w];//dat=Feature_in_dache[cin*Hin*Win+h*Win+w];
//
//								// INT16*K weight[CHout][Ky][Kx][CHin/K]为权重的形状，[K]相当于INT16*K??
//								// INT16*K weight[CHout][CHin/K][Ky][Kx][CHin/K]为权重的形状，[K]相当于INT16*K??
//								// wt=weight[cout][ii][jj][cin]为这个形状里取的索引
//
//								//wt=W[cout*CHin_div_K*Kx*Ky+ii*CHin_div_K*Kx+jj*CHin_div_K+cin];
//
//								wt=W[cout*1*3*3+cin*3*3+ii*3+jj];//wt=W[cout*CHin_div_K*Kx*Ky+cin*Kx*Ky+ii*Kx+jj];
//
//								//std::cout<<"dat="<<dat<<",W="<<wt<<std::endl;
//								tp.range(31,0)=(dtype_dat)dat.range(15,0)*(dtype_dat)wt.range(15,0);
//								sum+=(dtype_mul)tp.range(31,0);
//							}
//							else
//							{
//								dat=0;
//								wt=0;
//							}
//							//std::cout<<"sum="<<sum<<std::endl;
//							if((cin==1-1) && (jj==3-1) && (ii==3-1) )//CHin_div_K Kx Ky
//							{
//								// 如果使用了非线???激活层，小??0就设??0
//								if(1 & sum<0)//relu_en
//									sum=0;
//								// 数据右移
//								// 右移位数为输入feature map小数点位??+权重小数点位??-输出feature map小数点位??
//								dtype_acc res=sum>>15;//out_truncate
//
//								// 移位如果范围，进行前后的钳位
//								if(res>32767)
//									res=32767;
//								else
//									if(res<-32768)
//										res=-32768;
//
//								dtype_dat res_16=res;
////								out_tp.range((cout%1)*16+15,(cout%1)*16)=res;sum=0;
////								// feature_out[cout][i][j][cout]
////								if( ((cout%1)==1-1) || (cout==(CHout-1)) )
////								{
////									feature_out[(cout/1)*Wout*Hout+i*Wout+j]=out_tp;out_tp=0;
////								}
//								out_tp.range(15,0)=res + B[cout];sum=0;
//								Feature_out_dache[cout*36*60+i*60+j]=out_tp;out_tp=0;
//								//Feature_out_dache[cout*Wout*Hout+i*Wout+j]=out_tp;out_tp=0;
//							 }
//						   }
//					}
//				}
//			}
//		}
//	}
////	memcpy((dtype_bus*)Feature_out_dache,feature_out,reg_3*sizeof(dtype_bus));
//}
