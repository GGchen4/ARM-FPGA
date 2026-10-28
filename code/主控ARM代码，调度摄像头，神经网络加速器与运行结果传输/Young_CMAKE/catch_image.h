#ifndef CATCH_IMAGE_H
#define CATCH_IMAGE_H

#include <qthread.h>
#include <qdebug.h>
#include <opencv2/core/core.hpp>
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

using namespace cv;
using namespace std;

// 宏定义（与原代码完全一致）
//#define SCALE 0.2
#define SCALE 1
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

// 外部声明互斥锁和人脸检测缓冲区
extern pthread_mutex_t catchLock;
extern unsigned char* pBuffer;

class catchImage : public QThread
{
    Q_OBJECT

public:
    catchImage(int channel = 0);
    ~catchImage();

    // 设置输入模式：0-摄像头，1-图片
    void setInputMode(int mode);
    void sendEyeCoords(const cv::Point3f& reye, const cv::Point3f& leye);
    void sendResults(int16_t result0, int16_t result1);

Q_SIGNALS:
    void sendImg(QImage *pImg);

private:
    void run(void) override;

    // 处理图片输入
    void processImage();

    int ch;                     // 摄像头通道
    int inputMode;              // 输入模式：0-摄像头，1-图片
    string imgPath;             // 图片路径
    int sockfd;                 // UDP套接字描述符
    struct sockaddr_in servaddr;// UDP服务端地址
};

class run_elf : public QThread
{
    Q_OBJECT

public:
    run_elf();

private:
    void run() override;
};

// 人脸检测函数（与原代码完全一致）
std::tuple<int, int, int, int, int, int, int, int, vector<Point2f>> libface_detection(Mat input_img);

// 辅助函数（与原代码一致）
void safe_imwrite(const std::string& path, cv::Mat img);
bool saveFaceToBin(const cv::Mat& inputMat);
IplImage *CutOut(IplImage *source_image, int x, int y, int w, int h);
cv::Mat CutOut_Mat(const cv::Mat& source_image, int x, int y, int w, int h);
QImage cvMatToQImage(const cv::Mat& mat);
int Devmem_Read(unsigned long readAddr, unsigned long* buf, unsigned long len, int status);
unsigned long get_register(unsigned long base_addr);
void write_register(unsigned long base_addr, unsigned long data);
int ddr_transfer(void *map_base0);
int action_elf(string command);
IplImage* input_img();
int16_t float_to_int16(float value, int n);
Mat convert_normalized_to_fixedpoint(cv::Mat& src, int n);
int16_t read_int16_from_addr(unsigned long addr);
int check_bit(unsigned long base_addr, unsigned long bit, int num);
int set_bit(unsigned long base_addr, unsigned long bit, int num);
void drawGaze(Mat& img, int bbox_x, int bbox_y, int bbox_w, int bbox_h,
              float pitch_rad, float yaw_rad,
              Scalar color = Scalar(0, 0, 255), int thickness = 2);
#endif // CATCH_IMAGE_H
