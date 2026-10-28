#pragma once
#include <ap_fixed.h>
#include <ap_int.h>
#include <cstring>

// =================== Data Types ===================
typedef ap_fixed<16,6,AP_RND,AP_SAT> data_t;
typedef ap_int<64> data_pack_t;
typedef ap_fixed<32,12, AP_RND, AP_SAT> prod_t; // 乘积足够：范围约±2048
typedef ap_fixed<40,20, AP_RND, AP_SAT> acc_t;  // 累加足够：范围约±524288

// =================== Tuned for 36x60 series ===================
// Output tile matches smallest fmap 9x15 exactly
#define Tr   9
#define Tc   15

// Channel tiling
#define Tm   16      // output-channel tile (fits Cout up to 32 in 2 blocks)
#define Tn   8       // input-channel tile (fits Cin up to 32 in 4 blocks)

// Packed output lanes
#define PACK 4       // 4 lanes x 16bit = 64bit

// stride only 1 or 2
#define MAX_STRIDE 2
#define MAX_IN_H (Tr*MAX_STRIDE + 2)  // Tr*stride + (K-1) with K=3 => +2 => 20
#define MAX_IN_W (Tc*MAX_STRIDE + 2)  // 32

// =================== Top ===================
void conv3x3_g_36x60(
  const data_t* in,     // layout: in[c*H*W + r*W + col]
  const data_t* w,      // layout: w[oc*Cin*9 + ic*9 + ky*3 + kx]
  const data_t* bias,   // bias[oc]
  data_pack_t* out,     // layout: out[(oc/4)*outH*outW + oh*outW + ow], pack4
  int H, int W,
  int Cin, int Cout,
  int stride,   // 1 or 2
  int pad ,     // 0 or 1
  int relu_en , // 0 or 1
  int bias_en   // 0 or 1
);
void conv36x60_case1(
  const data_t* in,
  const data_t* w,
  const data_t* bias,
  data_pack_t* out
);
static inline data_t relu6(data_t x){
#pragma HLS INLINE
  if(x < (data_t)0) return (data_t)0;
//  if(x > (data_t)6) return (data_t)6;
  return x;
}
