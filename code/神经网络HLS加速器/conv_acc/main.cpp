#include "conv_core.h"
#include <iostream>
#include <vector>
#include <cstdlib>

using namespace std;

static void fill_det(data_t* x, int length){
  for(int i=0;i<length;i++){
    x[i] = (data_t)(((i*i*i - 5*i*i + 7*i + 13) % 100000 - 50000) / 50000.0);
  }
}

static inline int out_dim(int in, int pad, int stride){
  // K=3 fixed
  return (in + 2*pad - 3)/stride + 1;
}

// golden conv3x3, CHW layout, w layout: oc*Cin*9 + ic*9 + ky*3 + kx
static void golden_conv3x3(
  const data_t* in,
  const data_t* w,
  const data_t* bias,
  data_t* out,        // out: Cout*outH*outW (not packed)
  int H, int W, int Cin, int Cout,
  int stride, int pad
){
  int outH = out_dim(H, pad, stride);
  int outW = out_dim(W, pad, stride);

  for(int oc=0; oc<Cout; oc++){
    for(int oh=0; oh<outH; oh++){
      for(int ow=0; ow<outW; ow++){
    	  acc_t acc = bias[oc];
        for(int ic=0; ic<Cin; ic++){
          for(int ky=0; ky<3; ky++){
            for(int kx=0; kx<3; kx++){
              int ih = oh*stride + ky - pad;
              int iw = ow*stride + kx - pad;
              if(ih>=0 && ih<H && iw>=0 && iw<W){
                data_t a = in[ic*H*W + ih*W + iw];
                data_t b = w[oc*Cin*9 + ic*9 + ky*3 + kx];
                acc += (acc_t)((prod_t)a * (prod_t)b);
//                acc += a*b;
              }
            }
          }
        }
        out[oc*outH*outW + oh*outW + ow] = relu6((data_t)acc);
      }
    }
  }
}

// unpack from packed output
static data_t unpack_lane(
  const data_pack_t* outp,
  int outH, int outW,
  int oc, int oh, int ow
){
  int ocg  = oc / PACK;
  int lane = oc % PACK;
  data_pack_t pack = outp[ocg*outH*outW + oh*outW + ow];
  ap_int<16> v16 = pack.range(16*lane+15, 16*lane);

  data_t v;
  v.range(15,0) = v16; // bit-cast back
  return v;
}

static bool run_one_case(int H, int W, int Cin, int Cout, int stride, int pad){
  int outH = out_dim(H, pad, stride);
  int outW = out_dim(W, pad, stride);

  // sizes
  long in_sz   = (long)Cin * H * W;
  long w_sz    = (long)Cout * Cin * 9;
  long bias_sz = (long)Cout;
  long outp_sz = (long)( (Cout + PACK - 1)/PACK ) * outH * outW; // packed words
  long outg_sz = (long)Cout * outH * outW; // golden scalar

  vector<data_t> in(in_sz);
  vector<data_t> w(w_sz);
  vector<data_t> bias(bias_sz);
  vector<data_pack_t> outp(outp_sz);
  vector<data_t> outg(outg_sz);

  fill_det(in.data(), (int)in_sz);
  fill_det(w.data(), (int)w_sz);
  fill_det(bias.data(), (int)bias_sz);

  // golden
  golden_conv3x3(in.data(), w.data(), bias.data(), outg.data(), H, W, Cin, Cout, stride, pad);

  // dut
  conv3x3_g_36x60(in.data(), w.data(), bias.data(), outp.data(), H, W, Cin, Cout, stride, pad,1,1);

  // compare
  for(int oc=0; oc<Cout; oc++){
    for(int oh=0; oh<outH; oh++){
      for(int ow=0; ow<outW; ow++){
        data_t dut = unpack_lane(outp.data(), outH, outW, oc, oh, ow);
        data_t ref = outg[oc*outH*outW + oh*outW + ow];
        if(dut != ref){
          cout << "Mismatch! "
               << "H="<<H<<" W="<<W<<" Cin="<<Cin<<" Cout="<<Cout
               << " stride="<<stride<<" pad="<<pad
               << " @oc="<<oc<<" oh="<<oh<<" ow="<<ow
               << " ref="<< (float)ref << " dut=" << (float)dut << endl;
          return false;
        }
      }
    }
  }

  cout << "PASS: "
       << "H="<<H<<" W="<<W<<" Cin="<<Cin<<" Cout="<<Cout
       << " stride="<<stride<<" pad="<<pad
       << " -> outH="<<outH<<" outW="<<outW
       << " packed_words="<<outp_sz
       << endl;
  return true;
}

int main(){
  // Your network-like cases:
  // 1) 36x60x1 -> 36x60x16 (stride=1 pad=1)
  if(!run_one_case(36, 60, 1, 16, 1, 1)) return -1;

  // 2) 36x60x16 -> 18x30x32 (stride=2 pad=1)
  if(!run_one_case(36, 60, 16, 32, 2, 1)) return -1;

  // 3) 18x30x32 -> 9x15x32 (stride=2 pad=1)
  if(!run_one_case(18, 30, 32, 32, 2, 1)) return -1;

  // Extra sanity cases (optional but useful)
  if(!run_one_case(36, 60, 3, 32, 1, 1)) return -1;   // common RGB-like
  if(!run_one_case(36, 60, 16, 16, 1, 0)) return -1;  // pad=0
  if(!run_one_case(9,  15, 16, 32, 1, 1)) return -1;  // smallest fmap stride1

  cout << "ALL TESTS PASS." << endl;
  return 0;
}
