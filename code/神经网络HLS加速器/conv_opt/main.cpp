#include "stdio.h"
#include "conv_core.h"
#include <iostream>
#include <vector>
#include <memory>
#include <algorithm>
#include <chrono>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <fstream>

#define IN_WIDTH 60
#define IN_HEIGHT 36
#define IN_CH 16
#define IN_CH_DIV_K ((IN_CH+K-1)/K)

#define KERNEL_WIDTH 3
#define KERNEL_HEIGHT 3
#define X_STRIDE 1
#define Y_STRIDE 1

#define RELU_EN  0
#define MODE     1          //0:VALID, 1:SAME
#define X_PADDING (MODE?(KERNEL_WIDTH-1)/2:0)
#define Y_PADDING (MODE?(KERNEL_HEIGHT-1)/2:0)

#define OUT_CH 3
#define OUT_CH_DIV_K ((OUT_CH+K-1)/K)
#define OUT_WIDTH ((IN_WIDTH+2*X_PADDING-KERNEL_WIDTH)/X_STRIDE+1)
#define OUT_HEIGHT ((IN_HEIGHT+2*Y_PADDING-KERNEL_HEIGHT)/Y_STRIDE+1)

#define MAX_SIZE 1048576 // 假设数组的最大大小为 1MB


int16_t float_to_int16(float value,int m, int n)
{
	float scaled = value * (1 << n);
	int16_t fixed_point = round(scaled);
	return fixed_point;
}

int main(void)
{
//	dtype_bus feature_in[2160];
//	dtype_bus Feature_in_dache[2160];
//	dtype_dat reg1 = 2160;

	dtype_bus W[2304];
//	dtype_bus W_dache[144];
//	dtype_dat reg2 = 144;

	dtype_bus feature_out[34560];
//	dtype_bus Feature_out_dache[34560];
//	dtype_dat reg3 = 34560;

	dtype_dat temp;


//	for(int cin=0;cin<IN_CH_DIV_K;cin++)
//		for(int i=0;i<IN_HEIGHT;i++)
//			for(int j=0;j<IN_WIDTH;j++)
//				for(int k=0;k<K;k++)
//					if((cin*K+k)<IN_CH){
////						feature_in[cin][i][j].range(16*k+15,16*k)=(1<<14);//i*IN_WIDTH+j;
//						feature_in[cin][i][j].range(16*k+15,16*k)=i+j;//i*IN_WIDTH+j;
//						feature_in[cin][i][j].range(16*k+15,16*k) = feature_in[cin][i][j].range(16*k+15,16*k)<<8;
//					}
//					else
//						feature_in[cin][i][j].range(16*k+15,16*k)=0;

     char CONV_WEIGHT_ADDR[MAX_SIZE]; // 声明字符数组
     char *stage1_conv1;
     float stage1_conv1_buffer = 0.0;
     int16_t stage1_conv1_Int_buffer = 0;
     int stage1_conv1_address = 0;

//     float img[2160]={
//     //	#include "C:\Users\Young\Desktop\conv_test\conv_test\src\weight\8_92.h"
//     	#include "./data.h"
//     	};
//     for(int cout=0;cout<2160;cout++){
//    	 stage1_conv1_Int_buffer = float_to_int16(img[cout],16,15);
//    	 feature_in[cout] = stage1_conv1_Int_buffer;
//     }

     dtype_bus feature_in[34560]={
     //	#include "C:\Users\Young\Desktop\conv_test\conv_test\src\weight\8_92.h"
     	#include "D:\Young\HLS\BN_Set_WBVM\output_stage2_bn1.h"
     	};

//	for(int cout=0;cout<2160;cout++)
//		{
//			std::cout<<"OUT["<<cout<<"]"<<"]="<<feature_in[cout]<<std::endl;
//		}
//	memcpy((dtype_bus *)Feature_in_dache, feature_in,reg1*sizeof(dtype_bus));

    float conv[4608]={
    //	#include "C:\Users\Young\Desktop\conv_test\conv_test\src\weight\8_92.h"
    	#include "./stage2_block1_conv1_weight.h"
    	};
    for(int cout=0;cout<4608;cout++){
    	stage1_conv1_Int_buffer = float_to_int16(conv[cout],16,15);
    	W[cout] = stage1_conv1_Int_buffer;
    }

//	for(int cout=0;cout<144;cout++)
//		{
//			std::cout<<"CONV["<<cout<<"]"<<"]="<<W[cout]<<std::endl;
//		}
//	memcpy((dtype_bus *)W_dache, W,reg2*sizeof(dtype_bus));

//	for(int cin=0;cin<IN_CH_DIV_K;cin++)
//		for(int i=0;i<IN_HEIGHT;i++)
//			for(int j=0;j<IN_WIDTH;j++)
//				for(int k=0;k<K;k++){
//					memcpy((dtype_bus *)Feature_in_dache++, feature_in[cin*IN_HEIGHT*IN_WIDTH+i*IN_WIDTH+j+k],reg1*sizeof(dtype_bus));
//				}

//	for(int cin=0;cin<IN_CH_DIV_K;cin++)
//		for(int i=0;i<IN_HEIGHT;i++)
//			for(int j=0;j<IN_WIDTH;j++)
//				for(int k=0;k<K;k++){
//					temp = feature_in[cin][i][j].range(16*k+15,16*k);
//					//printf("feature_in[%d][%d][%d][%d] = %d \n", cin, i, j, k, temp);
//					std::cout<<"feature_in["<<cin<<"]["<<i<<"]["<<j<<"]["<<k<<"]="<<(dtype_dat)temp<<std::endl;
//				}
//
//
//
//	for(int i=0;i<KERNEL_HEIGHT;i++)
//		for(int j=0;j<KERNEL_WIDTH;j++)
//			for(int cin=0;cin<IN_CH_DIV_K;cin++)
//				for(int cout=0;cout<OUT_CH;cout++)
//					for(int k=0;k<K;k++)
//						W[cout][i][j][cin].range(16*k+15,16*k)=(1<<8);//(i*KERNEL_WIDTH+j);//(cout==0)?(i*KERNEL_WIDTH+j):0;
//
//	memcpy((dtype_bus *)W_dache, W,reg2*sizeof(dtype_bus));
//
////	for(int cout=0;cout<OUT_CH;cout++)
////		for(int i=0;i<KERNEL_HEIGHT;i++)
////			for(int j=0;j<KERNEL_WIDTH;j++)
////				for(int cin=0;cin<IN_CH_DIV_K;cin++)
////					for(int k=0;k<K;k++){
////						memcpy((dtype_bus *)W_dache++, W[cout*IN_CH_DIV_K*KERNEL_HEIGHT*KERNEL_WIDTH+i*IN_CH_DIV_K*KERNEL_WIDTH+j*IN_CH_DIV_K+cin+k],reg2*sizeof(dtype_bus));
////					}
//	/*打印测试
//	for(int i=0;i<KERNEL_HEIGHT;i++)
//			for(int j=0;j<KERNEL_WIDTH;j++)
//				for(int cin=0;cin<IN_CH_DIV_K;cin++)
//					for(int cout=0;cout<OUT_CH;cout++)
//						for(int k=0;k<K;k++){
//							temp = W[cout][i][j][cin].range(16*k+15,16*k);
//							std::cout<<"W["<<cin<<"]["<<i<<"]["<<j<<"]["<<k<<"]="<<(dtype_dat)temp<<std::endl;
//						}
//	*/
//
//
//	for(int cout=0;cout<OUT_CH_DIV_K;cout++)
//		for(int i=0;i<OUT_HEIGHT;i++)
//			for(int j=0;j<OUT_WIDTH;j++)
//				feature_out[cout][i][j]=0;
//
//	memcpy((dtype_bus *)Feature_out_dache, feature_out, reg3*sizeof(dtype_bus));
//
////	for(int cout=0;cout<OUT_CH_DIV_K;cout++)
////		for(int i=0;i<OUT_HEIGHT;i++)
////			for(int j=0;j<OUT_WIDTH;j++){
////				memcpy((dtype_bus *)Feature_out_dache++, feature_out[cout*OUT_HEIGHT*OUT_WIDTH+i*OUT_WIDTH+j], reg3*sizeof(dtype_bus));
////			}
//
//
//	printf("1234\n");
//


	for(int cout=0;cout<17280;cout++)
		{
			feature_out[cout] = 0;
		}
	Conv(16,36,60,32,3,3,2,2,1,0,feature_in,15,W,15,feature_out,15);//mode: 0:VALID, 1:SAME
//
//	dtype_bus short_cut[34560];
//	dtype_bus BN1_1X1[34560];
////	dtype_bus weight_1X1[512];
////    float conv_1x1[512]={
////    //	#include "C:\Users\Young\Desktop\conv_test\conv_test\src\weight\8_92.h"
////    	#include "./stage2_block1_shortcut_conv_weight.h"
////    	};
////    for(int cout=0;cout<2304;cout++){
////    	stage1_conv1_Int_buffer = float_to_int16(conv_1x1[cout],16,15);
////    	weight_1X1[cout] = stage1_conv1_Int_buffer;
////    }
//    dtype_bus BN1[34560]={
//    //	#include "C:\Users\Young\Desktop\conv_test\conv_test\src\weight\8_92.h"
//    	#include "D:\Young\HLS\BN_Set_WBVM\output_stage1_bn1.h"
//    	};
//
//	for(int cout=0;cout<34560;cout++)
//		{
//			short_cut[cout] = feature_out[cout] + BN1[cout];
//		}


	for(int cout=0;cout<17280;cout++)
	{
		std::cout<<feature_out[cout]<<","<<std::endl;
//		std::cout<<short_cut[cout]<<","<<std::endl;
	}


	return 0;
}

