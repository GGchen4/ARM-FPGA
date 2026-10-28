#include "conv_core.h"

// small helper
static inline int min_i(int a, int b){ return (a < b) ? a : b; }

// ------------------------ load input tile (ping/pong) ------------------------
static void load_input_tile(
  const data_t* in,
  data_t in_buf[Tn][MAX_IN_H][MAX_IN_W],
  int H, int W,
  int n_base, int curTn,
  int oh_base, int ow_base,
  int stride, int pad
){
#pragma HLS INLINE off
#pragma HLS ARRAY_PARTITION variable=in_buf complete dim=1

  const int in_h = Tr*stride + 2;  // +2 because K=3
  const int in_w = Tc*stride + 2;

  const int ih0 = oh_base*stride - pad;
  const int iw0 = ow_base*stride - pad;

  for(int nn=0; nn<Tn; nn++){
    for(int i=0; i<in_h; i++){
      for(int j=0; j<in_w; j++){
#pragma HLS PIPELINE II=1
        data_t v = (data_t)0;
        if(nn < curTn){
          int ih = ih0 + i;
          int iw = iw0 + j;
          if(ih>=0 && ih<H && iw>=0 && iw<W){
            int c = n_base + nn;
            v = in[c*H*W + ih*W + iw];
          }
        }
        in_buf[nn][i][j] = v;
      }
    }
  }
}

// ------------------------ load weight tile ------------------------
static void load_weight_tile(
  const data_t* w,
  data_t w_buf[Tm][Tn][3][3],
  int Cin,
  int m_base, int curTm,
  int n_base, int curTn
){
#pragma HLS INLINE off
#pragma HLS ARRAY_PARTITION variable=w_buf complete dim=1
#pragma HLS ARRAY_PARTITION variable=w_buf complete dim=2

  for(int mm=0; mm<curTm; mm++){
    for(int nn=0; nn<Tn; nn++){
      for(int ky=0; ky<3; ky++){
        for(int kx=0; kx<3; kx++){
#pragma HLS PIPELINE II=1
          int oc = m_base + mm;
          int ic = n_base + nn;
          if(nn<curTn){
        	  w_buf[mm][nn][ky][kx] = w[oc*Cin*9 + ic*9 + ky*3 + kx];
          }
          else{
        	  w_buf[mm][nn][ky][kx] = (data_t)0;
          }

        }
      }
    }
  }
}

// ------------------------ init out tile with bias ------------------------
static void init_out_with_bias(
		acc_t out_buf[Tm][Tr][Tc],
  const data_t* bias,
  int m_base, int curTm
){
#pragma HLS INLINE off
#pragma HLS ARRAY_PARTITION variable=out_buf complete dim=1

  for(int mm=0; mm<curTm; mm++){
    for(int i=0;i<Tr;i++){
      for(int j=0;j<Tc;j++){
#pragma HLS PIPELINE II=1
        out_buf[mm][i][j] = (acc_t)bias[m_base + mm];
      }
    }
  }
}

// ------------------------ compute accumulate ------------------------
//static void compute_tile_acc(
//  data_t in_buf[Tn][MAX_IN_H][MAX_IN_W],
//  data_t w_buf[Tm][Tn][3][3],
//  acc_t out_acc[Tm][Tr][Tc],
//  int curTm, int curTn,
//  int stride
//){
//#pragma HLS INLINE off
//#pragma HLS ARRAY_PARTITION variable=in_buf  complete dim=1
//#pragma HLS ARRAY_PARTITION variable=w_buf   complete dim=1
//#pragma HLS ARRAY_PARTITION variable=w_buf   complete dim=2
//#pragma HLS ARRAY_PARTITION variable=out_acc complete dim=1
//
//  // Order chosen to keep in_buf addressing simple
//  for(int oh=0; oh<Tr; oh++){
//    for(int ow=0; ow<Tc; ow++){
//      for(int ky=0; ky<3; ky++){
//        for(int kx=0; kx<3; kx++){
//#pragma HLS PIPELINE II=1
//          int ih = oh*stride + ky;
//          int iw = ow*stride + kx;
//
//          for(int mm=0; mm<Tm; mm++){
//#pragma HLS UNROLL factor=4
////            data_t acc = out_buf[mm][oh][ow];
//            acc_t acc = out_acc[mm][oh][ow];
//            for(int nn=0; nn<Tn; nn++){
//#pragma HLS UNROLL
////              acc += in_buf[nn][ih][iw] * w_buf[mm][nn][ky][kx];
//              prod_t p = (prod_t)in_buf[nn][ih][iw] * (prod_t)w_buf[mm][nn][ky][kx];
//              acc += (acc_t)p;
//            }
//            out_acc[mm][oh][ow] = acc;
//          }
//        }
//      }
//    }
//  }
//}

//static void compute_tile_acc(
//  data_t in_buf[Tn][MAX_IN_H][MAX_IN_W],
//  data_t w_buf[Tm][Tn][3][3],
//  acc_t  out_acc[Tm][Tr][Tc],
//  int curTm, int curTn,
//  int stride
//){
//#pragma HLS INLINE off
//#pragma HLS ARRAY_PARTITION variable=in_buf  complete dim=1
//#pragma HLS ARRAY_PARTITION variable=w_buf   complete dim=1
//#pragma HLS ARRAY_PARTITION variable=w_buf   complete dim=2
//#pragma HLS ARRAY_PARTITION variable=out_acc complete dim=1
//
//  for(int oh=0; oh<Tr; oh++){
//    for(int ow=0; ow<Tc; ow++){
//#pragma HLS PIPELINE II=1
//
//      // 对每个mm做累加（mm并行4路）
//      for(int mm=0; mm<Tm; mm++){
//#pragma HLS UNROLL factor=4
////        if(mm >= curTm) continue;
//
//        acc_t acc = out_acc[mm][oh][ow];   // 只读一次
//
//        // 3x3*curTn 累加在寄存器里做完
//        for(int nn=0; nn<Tn; nn++){
//#pragma HLS UNROLL
////          if(nn >= curTn) continue;
//
//          // 这里 ky/kx 建议先只展开 kx，ky串行（更容易过时序）
//          for(int ky=0; ky<3; ky++){
////#pragma HLS UNROLL   // 如果你时序过不了就去掉这个
//            for(int kx=0; kx<3; kx++){
////#pragma HLS UNROLL
//              int ih = oh*stride + ky;
//              int iw = ow*stride + kx;
//              prod_t p = (prod_t)in_buf[nn][ih][iw] * (prod_t)w_buf[mm][nn][ky][kx];
//              acc += (acc_t)p;
//            }
//          }
//        }
//
//        out_acc[mm][oh][ow] = acc;         // 只写一次（关键）
//      }
//    }
//  }
//}

static void compute_tile_acc(
  data_t in_buf[Tn][MAX_IN_H][MAX_IN_W],
  data_t w_buf[Tm][Tn][3][3],
  acc_t  out_acc[Tm][Tr][Tc],
  int curTm, int curTn,
  int stride
){
#pragma HLS INLINE off
#pragma HLS ARRAY_PARTITION variable=in_buf  complete dim=1
#pragma HLS ARRAY_PARTITION variable=w_buf   complete dim=1
#pragma HLS ARRAY_PARTITION variable=w_buf   complete dim=2
#pragma HLS ARRAY_PARTITION variable=out_acc complete dim=1
	acc_t tmp=0;
	for(int kx=0;kx<3;kx++)
		for(int ky=0;ky<3;ky++)
			for(int oh=0;oh<9;oh++)
				for(int ow=0;ow<15;ow++){
#pragma HLS PIPELINE II=1
					for(int mm=0;mm<16;mm++){
						for(int nn=0;nn<8;nn++){
				            int ih = oh*stride + ky;
				            int iw = ow*stride + kx;
							tmp=out_acc[mm][oh][ow];
							prod_t p = (prod_t)in_buf[nn][ih][iw] * (prod_t)w_buf[mm][nn][ky][kx];
							tmp += (acc_t)p;
							out_acc[mm][oh][ow]=tmp;
						}
					}
				}
}

// ------------------------ store output tile (pack4) ------------------------

// ------------------------ store output tile (pack4) ------------------------
// Assumptions:
//   - PACK = 4
//   - out_acc: [Tm=16][Tr=9][Tc=15] (tile-local coordinates)
//   - out is packed layout: out[ocg*outH*outW + oh*outW + ow]
static void store_output_tile_pack4(
  data_pack_t* out,
  acc_t out_acc[Tm][Tr][Tc],
  int outH, int outW,
  int m_base, int curTm,
  int oh_base, int ow_base
){
#pragma HLS INLINE off
#pragma HLS ARRAY_PARTITION variable=out_acc complete dim=1

  // 计算该 tile 在输出图上的有效范围（裁剪边界），避免内层 if(oh<outH && ow<outW)
  int oh_end = oh_base + Tr;
  int ow_end = ow_base + Tc;
  if(oh_end > outH) oh_end = outH;
  if(ow_end > outW) ow_end = outW;

  // 该 tile 有效高宽
  int eff_h = oh_end - oh_base;   // 0..Tr
  int eff_w = ow_end - ow_base;   // 0..Tc

  // 以 PACK 为步长写回
  for(int mm = 0; mm < curTm; mm += PACK){
    // 该 pack4 对应的输出通道组
    int ocg = (m_base + mm) / PACK;

    // ocg 平面的基址：out[ocg][0][0]
    int plane_base = ocg * (outH * outW);

    // 遍历 tile 有效区域：注意这里用 eff_h/eff_w，内层无边界判断，写地址规则
    for(int i = 0; i < eff_h; i++){
      // 当前输出行号
      int oh = oh_base + i;

      // 该行起始地址：out[ocg][oh][ow_base]
      int row_base = plane_base + oh * outW + ow_base;

      for(int j = 0; j < eff_w; j++){
#pragma HLS PIPELINE II=1
        // 当前输出列号
        // int ow = ow_base + j; // 仅用于理解，不必显式算 ow

        // pack4
        data_pack_t pack = 0;

        // lane 打包：mm+0..mm+3
        for(int lane = 0; lane < PACK; lane++){
#pragma HLS UNROLL
          ap_int<16> v16 = 0;

          if((mm + lane) < curTm){
            // out_acc 是 tile-local 坐标：i/j 对应 tile 内位置
            data_t v = (data_t)out_acc[mm + lane][i][j]; // 在写回阶段再截断
            v = relu6(v);
            v16 = v.range(15, 0);
          }
          pack.range(16*lane + 15, 16*lane) = v16;
        }

        // 线性地址写回：row_base 在 j 循环里严格递增
        out[row_base + j] = pack;
      }
    }
  }
}

//static void store_output_tile_pack4(
//  data_pack_t* out,
//  acc_t out_acc[Tm][Tr][Tc],
//  int outH, int outW,
//  int m_base, int curTm,
//  int oh_base, int ow_base
//){
//#pragma HLS INLINE off
//#pragma HLS ARRAY_PARTITION variable=out_acc complete dim=1
//
//  for(int mm=0; mm<curTm; mm+=PACK){
//    for(int i=0;i<Tr;i++){
//      for(int j=0;j<Tc;j++){
//#pragma HLS PIPELINE II=1
//        int oh = oh_base + i;
//        int ow = ow_base + j;
//        if(oh < outH && ow < outW){
//          data_pack_t pack = 0;
//          for(int lane=0; lane<PACK; lane++){
//#pragma HLS UNROLL
//            ap_int<16> v16 = 0;
//            if((mm + lane) < curTm){
////              data_t v = relu6(out_buf[mm+lane][i][j]);
//              data_t v = (data_t)out_acc[mm+lane][i][j]; // 只在这里截断一次
//              v = relu6(v);
//              v16 = v.range(15,0);
//            }
//            pack.range(16*lane+15, 16*lane) = v16;
//          }
//          int ocg = (m_base + mm) / PACK;
//          out[ocg*outH*outW + oh*outW + ow] = pack;
//        }
//      }
//    }
//  }
//}

static void load_input_full(
  const data_t* in,
  data_t in_local[16*36*60],
  int H, int W, int Cin
){
#pragma HLS INLINE off
  for(int c=0;c<Cin;c++){
    for(int i=0;i<H;i++){
      for(int j=0;j<W;j++){
#pragma HLS PIPELINE II=1
        in_local[c*H*W + i*W + j] = in[c*H*W + i*W + j];
      }
    }
  }
}
// =====================================================================
//                        TOP: conv3x3_g_36x60
// =====================================================================
void conv3x3_g_36x60(
  const data_t* in,
  const data_t* w,
  const data_t* bias,
  data_pack_t* out,
  int H, int W,
  int Cin, int Cout,
  int stride,
  int pad ,
  int relu_en ,
  int bias_en
){
#pragma HLS INTERFACE s_axilite port=return bundle=CTRL

#pragma HLS INTERFACE m_axi port=in   offset=slave bundle=IN  max_read_burst_length=64
#pragma HLS INTERFACE m_axi port=w    offset=slave bundle=W   max_read_burst_length=64

#pragma HLS INTERFACE m_axi port=bias offset=slave bundle=W   max_read_burst_length=64
//#pragma HLS INTERFACE m_axi port=out  offset=slave bundle=OUT max_write_burst_length=64
#pragma HLS INTERFACE m_axi port=out offset=slave bundle=OUT \
  max_write_burst_length=64 num_write_outstanding=16

//#pragma HLS INTERFACE bram port=bias
//#pragma HLS INTERFACE bram port=out

#pragma HLS INTERFACE s_axilite port=H      bundle=CTRL
#pragma HLS INTERFACE s_axilite port=W      bundle=CTRL
#pragma HLS INTERFACE s_axilite port=Cin    bundle=CTRL
#pragma HLS INTERFACE s_axilite port=Cout   bundle=CTRL
#pragma HLS INTERFACE s_axilite port=stride bundle=CTRL
#pragma HLS INTERFACE s_axilite port=pad    bundle=CTRL
#pragma HLS INTERFACE s_axilite port=relu_en bundle=CTRL
#pragma HLS INTERFACE s_axilite port=bias_en    bundle=CTRL
  // sanitize
  int st = (stride == 2) ? 2 : 1;
  int pd = (pad != 0) ? 1 : 0;

  // output shape
  int outH = (H + 2*pd - 3) / st + 1;
  int outW = (W + 2*pd - 3) / st + 1;

  // on-chip buffers
  data_t in_buf_ping[Tn][MAX_IN_H][MAX_IN_W];
  data_t in_buf_pong[Tn][MAX_IN_H][MAX_IN_W];
#pragma HLS ARRAY_PARTITION variable=in_buf_ping complete dim=1
#pragma HLS ARRAY_PARTITION variable=in_buf_pong complete dim=1

  data_t w_buf[Tm][Tn][3][3];
#pragma HLS ARRAY_PARTITION variable=w_buf complete dim=1
#pragma HLS ARRAY_PARTITION variable=w_buf complete dim=2

  acc_t out_buf[Tm][Tr][Tc];

#pragma HLS ARRAY_PARTITION variable=out_buf complete dim=1

//  data_t in[16*36*60];
//#pragma HLS RESOURCE variable=in core=RAM_2P_BRAM
//  load_input_full(in_local,in,H,W,Cin);

  // scan output tiles
  for(int oh_base=0; oh_base<outH; oh_base+=Tr){
    for(int ow_base=0; ow_base<outW; ow_base+=Tc){
      // output channel blocks
      for(int m_base=0; m_base<Cout; m_base+=Tm){
        int curTm = min_i(Tm, Cout - m_base);

        // init output tile with bias
        init_out_with_bias(out_buf, bias, m_base, curTm);

        // ping-pong input tiles over Cin blocks
        bool ping = true;

        // preload first Cin block into ping
        int n0 = 0;
        int curTn0 = min_i(Tn, Cin - n0);
        load_input_tile(in, in_buf_ping, H, W, n0, curTn0, oh_base, ow_base, st, pd);

        for(int n_base=0; n_base<Cin; n_base+=Tn){
          int curTn = min_i(Tn, Cin - n_base);

          // load weights for this (m_base, n_base)
          load_weight_tile(w, w_buf, Cin, m_base, curTm, n_base, curTn);

          // prefetch next Cin block input tile
          int n_next = n_base + Tn;
          if(n_next < Cin){
            int curTn_next = min_i(Tn, Cin - n_next);
            if(ping){
              load_input_tile(in, in_buf_pong, H, W, n_next, curTn_next, oh_base, ow_base, st, pd);
            }else{
              load_input_tile(in, in_buf_ping, H, W, n_next, curTn_next, oh_base, ow_base, st, pd);
            }
          }

          // compute on current buffer
          if(ping){
            compute_tile_acc(in_buf_ping, w_buf, out_buf, curTm, curTn, st);
          }else{
            compute_tile_acc(in_buf_pong, w_buf, out_buf, curTm, curTn, st);
          }

          ping = !ping;
        }

        // store tile
        store_output_tile_pack4(out, out_buf, outH, outW, m_base, curTm, oh_base, ow_base);
      }
    }
  }
}
void conv36x60x16_36x60x16(
  const data_t* in,
  const data_t* w,
  const data_t* bias,
  data_pack_t* out
){
#pragma HLS INTERFACE s_axilite port=return bundle=CTRL
#pragma HLS INTERFACE m_axi port=in   bundle=IN
#pragma HLS INTERFACE m_axi port=w    bundle=W

#pragma HLS INTERFACE m_axi port=bias bundle=W
#pragma HLS INTERFACE m_axi port=out  bundle=OUT

//#pragma HLS INTERFACE bram port=bias
//#pragma HLS INTERFACE bram port=out


  conv3x3_g_36x60(in, w, bias, out,
                  36, 60,   // H W 固定
                  16, 16,     // Cin Cout 固定
                  1, 1,
				  1,1);     // stride pad 固定
}
void conv36x60x1_36x60x16(
  const data_t* in,
  const data_t* w,
  const data_t* bias,
  data_pack_t* out
){
#pragma HLS INTERFACE s_axilite port=return bundle=CTRL
#pragma HLS INTERFACE m_axi port=in   bundle=IN
#pragma HLS INTERFACE m_axi port=w    bundle=W

#pragma HLS INTERFACE m_axi port=bias bundle=W
#pragma HLS INTERFACE m_axi port=out  bundle=OUT

//#pragma HLS INTERFACE bram port=bias
//#pragma HLS INTERFACE bram port=out


  conv3x3_g_36x60(in, w, bias, out,
                  36, 60,   // H W 固定
                  1, 16,     // Cin Cout 固定
                  1, 1,
				  1,1);     // stride pad 固定
}
void conv36x60x16_18x30x32(
  const data_t* in,
  const data_t* w,
  const data_t* bias,
  data_pack_t* out
){
#pragma HLS INTERFACE s_axilite port=return bundle=CTRL
#pragma HLS INTERFACE m_axi port=in   bundle=IN
#pragma HLS INTERFACE m_axi port=w    bundle=W

#pragma HLS INTERFACE m_axi port=bias bundle=W
#pragma HLS INTERFACE m_axi port=out  bundle=OUT

//#pragma HLS INTERFACE bram port=bias
//#pragma HLS INTERFACE bram port=out


  conv3x3_g_36x60(in, w, bias, out,
                  36, 60,   // H W 固定
                  16, 32,     // Cin Cout 固定
                  2, 1,
				  1,1);     // stride pad 固定
}
void conv18x30x32_18x30x32(
  const data_t* in,
  const data_t* w,
  const data_t* bias,
  data_pack_t* out
){
#pragma HLS INTERFACE s_axilite port=return bundle=CTRL
#pragma HLS INTERFACE m_axi port=in   bundle=IN
#pragma HLS INTERFACE m_axi port=w    bundle=W

#pragma HLS INTERFACE m_axi port=bias bundle=W
#pragma HLS INTERFACE m_axi port=out  bundle=OUT

//#pragma HLS INTERFACE bram port=bias
//#pragma HLS INTERFACE bram port=out


  conv3x3_g_36x60(in, w, bias, out,
                  18, 30,   // H W 固定
                  32, 32,     // Cin Cout 固定
                  1, 1,
				  1,1);     // stride pad 固定
}
void conv18x30x32_9x15x64(
  const data_t* in,
  const data_t* w,
  const data_t* bias,
  data_pack_t* out
){
#pragma HLS INTERFACE s_axilite port=return bundle=CTRL
#pragma HLS INTERFACE m_axi port=in   bundle=IN
#pragma HLS INTERFACE m_axi port=w    bundle=W

#pragma HLS INTERFACE m_axi port=bias bundle=W
#pragma HLS INTERFACE m_axi port=out  bundle=OUT

//#pragma HLS INTERFACE bram port=bias
//#pragma HLS INTERFACE bram port=out


  conv3x3_g_36x60(in, w, bias, out,
                  18, 30,   // H W 固定
                  32, 64,     // Cin Cout 固定
                  2, 1,
				  1,1);     // stride pad 固定
}
void conv9x15x64_9x15x64(
  const data_t* in,
  const data_t* w,
  const data_t* bias,
  data_pack_t* out
){
#pragma HLS INTERFACE s_axilite port=return bundle=CTRL
#pragma HLS INTERFACE m_axi port=in   bundle=IN
#pragma HLS INTERFACE m_axi port=w    bundle=W

#pragma HLS INTERFACE m_axi port=bias bundle=W
#pragma HLS INTERFACE m_axi port=out  bundle=OUT

//#pragma HLS INTERFACE bram port=bias
//#pragma HLS INTERFACE bram port=out


  conv3x3_g_36x60(in, w, bias, out,
                  9, 15,   // H W 固定
                  64, 64,     // Cin Cout 固定
                  1, 1,
				  1,1);     // stride pad 固定
}
