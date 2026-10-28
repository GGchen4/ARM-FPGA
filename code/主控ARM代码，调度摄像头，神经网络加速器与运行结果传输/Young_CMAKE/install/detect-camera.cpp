#include <stdio.h>
#include <opencv2/opencv.hpp>
#include "facedetectcnn.h"

#include <linux/fb.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/ioctl.h>

#define DETECT_BUFFER_SIZE 0x9000
#define CUT_THRESHOLD 8

using namespace cv;
using namespace std;

int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        printf("Usage: %s <camera index>\n", argv[0]);
        return -1;
    }

    // 打开 framebuffer
    int fbfd = open("/dev/fb0", O_RDWR);
    if (fbfd < 0) {
        printf("Cannot open /dev/fb0\n");
        return -1;
    }

    struct fb_fix_screeninfo finfo;
    struct fb_var_screeninfo vinfo;

    ioctl(fbfd, FBIOGET_FSCREENINFO, &finfo);
    ioctl(fbfd, FBIOGET_VSCREENINFO, &vinfo);

    long screensize = vinfo.yres * finfo.line_length;
    uint8_t* fbp = (uint8_t*)mmap(0, screensize,
        PROT_READ | PROT_WRITE,
        MAP_SHARED, fbfd, 0);

    if ((int)fbp == -1) {
        printf("Framebuffer mmap failed\n");
        return -1;
    }

    printf("Framebuffer: %dx%d  line_length=%d  bpp=%d\n",
        vinfo.xres, vinfo.yres, finfo.line_length, vinfo.bits_per_pixel);

    // 打开摄像头
    VideoCapture cap;
    if (isdigit(argv[1][0]))
    {
        cap.open(argv[1][0] - '0');
        if (!cap.isOpened())
        {
            printf("Cannot open camera %s\n", argv[1]);
            return -1;
        }
    }

    int cam_w = cap.get(CAP_PROP_FRAME_WIDTH);
    int cam_h = cap.get(CAP_PROP_FRAME_HEIGHT);

    printf("Camera resolution: %d x %d\n", cam_w, cam_h);

    unsigned char* pBuffer = (unsigned char*)malloc(DETECT_BUFFER_SIZE);

    Mat frame, small, bgra;

    while (true)
    {
        cap >> frame;
        if (frame.empty()) continue;

        // 人脸检测输入缩小为 1/8
        resize(frame, small, Size(), 0.125, 0.125);

        TickMeter tm;
        tm.start();
        int* pResults = facedetect_cnn(pBuffer, (unsigned char*)small.ptr(0),
            small.cols, small.rows, small.step);
        tm.stop();

        printf("Detection time = %.2f ms    faces = %d\n",
            tm.getTimeMilli(), pResults ? *pResults : 0);

        // 在原图上画框
        for (int i = 0; i < (pResults ? *pResults : 0); i++)
        {
            short* p = ((short*)(pResults + 1)) + 16 * i;
            int conf = p[0];
            if (conf < 80) continue;

            int x = p[1] * CUT_THRESHOLD;
            int y = p[2] * CUT_THRESHOLD;
            int w = p[3] * CUT_THRESHOLD;
            int h = p[4] * CUT_THRESHOLD;

            rectangle(frame, Rect(x, y, w, h), Scalar(0, 255, 0), 2);
        }

        // 转成 BGRA（你的fb是 BGRA8888）
        cvtColor(frame, bgra, cv::COLOR_BGR2BGRA);

        // 只输出 640x480 到左上角
        for (int y = 0; y < cam_h; y++)
        {
            memcpy(fbp + y * finfo.line_length,
                bgra.ptr(y),
                cam_w * 4);   // BGRA 4 bytes
        }
    }

    free(pBuffer);
    munmap(fbp, screensize);
    close(fbfd);
    return 0;
}