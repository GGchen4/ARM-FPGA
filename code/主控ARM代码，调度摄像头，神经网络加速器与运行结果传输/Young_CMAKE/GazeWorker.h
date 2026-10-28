// GazeWorker.h
#ifndef GAZEWORKER_H
#define GAZEWORKER_H
//#include <turbojpeg.h>
#include <opencv2/core/core.hpp>
#include <opencv2/opencv.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv/cv.h>
#include <facedetectcnn.h>
#include <tuple>
#include <vector>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <iomanip>
#include <fstream>
#include <linux/videodev2.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <cstdlib>      // system
#include <cstring>      // strerror
#include <iostream>     // cout, cerr
#include <sys/stat.h>   // stat, chmod
#include <errno.h>      // errno
#include <wait.h>       // WIFEXITED, WEXITSTATUS
#include "get_eye_roi.h"
using namespace cv;
using namespace std;
// 你自己的 mean/std
// 宏定义（与原代码完全一致）
//#define SCALE 0.2
#define SCALE 0.13
#define CUT_THRESHOLD (1.0 / SCALE)


#define IMAGEWIDTH 640
#define IMAGEHEIGHT 480

#define IMG_CACHE_MAX 2

#define EYE_SCALE_X 5
#define EYE_SCALE_Y 4


#define DATA_LEN 10
#define MAP_SIZE 4096UL
#define MAP_MASK (4096 - 1)
#define DETECT_BUFFER_SIZE 0x9000
#define BRAM_CTRL_0 0x30001000
#define FRAME_BASE 0x11000000
#define TRANSFER_ADDR 0x13000000
#define GAZEXY_ADDR 0x02C00000

// 预处理参数（与Python一致）
#define RESIZE_MIN_SIDE 448
const float NORM_MEAN[3] = {0.485f, 0.456f, 0.406f};  // RGB均值
const float NORM_STD[3] = {0.229f, 0.224f, 0.225f};   // RGB标准差
bool processFrame(const cv::Mat &bgr_frame, 
                              cv::Mat &output_frame, 
                              int &face_x,
                              int &face_y,
                              int &face_w,
                              int &face_h,
                              int &theta, 
                              int &phi,unsigned char* pBuffer,void* input_buffer,cv::Mat &normalizing_rot,float& eye_center_x,float& eye_center_y,float& eye_center_z) ;
cv::Mat preprocess_face(const cv::Mat &face_bgr);
cv::Mat CutOut_Mat(const cv::Mat &img, int x, int y, int w, int h);
std::tuple<int, int, int, int, int, int, int, int, std::vector<cv::Point2f>> libface_detection(cv::Mat input_img,unsigned char* pBuffer);
bool saveFaceToBin(const cv::Mat &input);
int action_elf(string elf);
int Devmem_Read_Float(unsigned long addr, float *out, int count);
void drawGaze(cv::Mat &img, int x, int y, int w, int h, float pitch, float yaw);
int16_t float_to_int16(float value, int n);
Mat convert_normalized_to_fixed(cv::Mat& src, int n);
int Devmem_Read(unsigned long readAddr, unsigned long* buf, unsigned long len, int status);
unsigned long get_register(unsigned long base_addr);
void write_register(unsigned long base_addr, unsigned long data);
int Devmem_Read_int16(unsigned long readAddr, int16_t* buf, unsigned long len ,int status);
#endif // GAZEWORKER_H
