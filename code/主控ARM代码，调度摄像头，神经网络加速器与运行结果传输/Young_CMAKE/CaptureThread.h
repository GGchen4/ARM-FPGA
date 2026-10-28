#ifndef CAPTURETHREAD_H
#define CAPTURETHREAD_H
#include <QDebug>
#include <QThread>
#include "mainwindow.h"
#include <QElapsedTimer>
#include <turbojpeg.h>
class CaptureThread : public QThread {
    Q_OBJECT
public:
    CaptureThread(v4l2_dev *dev, int width, int height)
        : dev(dev), running(true), width(width), height(height) {}

    void stop() { running = false; }

signals:
    void frameReady(const cv::Mat &frame);

protected:
    void run() override {
        QElapsedTimer timer;
        int frames = 0;
        timer.start();

        // 预先分配一个 vector，减少 malloc/free
        std::vector<uchar> jpeg_copy;
        jpeg_copy.reserve(1024 * 1024);  // 预留 1MB，可按分辨率调大

        while (running) {
            // 1. 取一帧（select + DQBUF）
            int ret = v4l2_capture(dev);
            if (ret <= 0) continue;

            int idx   = dev->cur_index;
            unsigned char* data = dev->vid_buf[idx].v4l2_buff;
            int len   = dev->vid_buf[idx].v4l2_buff_length;  // 这里是 bytesused

            // 2. 拷贝 JPEG 数据到用户缓冲（保证后面 QBUF 后也安全）
            jpeg_copy.assign(data, data + len);

            // 3. 立刻把 buffer 还给驱动继续采集（保持流水线！）
            v4l2_requeue(dev, idx);

            // 4. 正确构造 Mat（1行 len 列，8bit 单通道）
            //imdecode解码
//            cv::Mat jpeg_mat(1, len, CV_8UC1, jpeg_copy.data());
//            cv::Mat frame = cv::imdecode(jpeg_mat, cv::IMREAD_COLOR);
            //turbojpeg解码
            tjhandle tj = tjInitDecompress();

            // 获取 JPEG 信息
            int width, height, subsamp, colorspace;
            tjDecompressHeader3(tj, jpeg_copy.data(), len,
                                &width, &height, &subsamp, &colorspace);

            // 创建 BGR Mat（OpenCV习惯用BGR）
            cv::Mat frame(height, width, CV_8UC3);

            // 解码 JPEG → BGR 图像
            tjDecompress2(
                tj, jpeg_copy.data(), len,
                frame.data,
                width, 0, height,
                TJPF_BGR,
                TJFLAG_FASTDCT     // 速度最快
            );

            tjDestroy(tj);  // 释放 handle
            //turbojpeg解码
            if (frame.empty()) {
                continue;  // 避免因少量坏帧导致闪屏
            }

            emit frameReady(frame);  // 或者 frame.clone()，看你 UI 那边怎么用

            frames++;
            if (timer.elapsed() >= 1000) {
                qDebug() << "MJPEG FPS:" << frames;
                frames = 0;
                timer.restart();
            }
        }
    }






private:
    v4l2_dev *dev;
    bool running;
    int width, height;
};

#endif // CAPTURETHREAD_H