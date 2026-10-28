#include "catch_image.h"
#include <qimage.h>
#include <opencv2/video/tracking.hpp>

#include "rw.h"
#include <QApplication>//为了 QApplication::quit();
#include <vector>// 用于bin文件写入和容器存储
#include <iomanip>// 用于bin文件写入和容器存储
pthread_mutex_t catchLock;

unsigned char *pBuffer = (unsigned char *)malloc(DETECT_BUFFER_SIZE);

// catchImage类实现
catchImage::catchImage(int channel) : QThread(0), ch(channel), inputMode(0), sockfd(-1)
{
    // 初始化UDP套接字（与原代码一致）
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        qDebug() << "Socket creation failed:" << strerror(errno);
        return;
    }
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(8888);
    inet_pton(AF_INET, "192.168.100.1", &servaddr.sin_addr);
}

catchImage::~catchImage()
{
    if (sockfd != -1) close(sockfd);
}

void catchImage::setInputMode(int mode)
{
    inputMode = mode;
    if (mode == 1) {
        imgPath = "./data.jpg";  // 默认图片路径（与原代码一致）
    }
}

void catchImage::run()
{
    if (inputMode == 1) {
        processImage();  // 处理图片
        return;
    }
    // 摄像头处理逻辑（与原代码完全一致）
    cv::VideoCapture    videoCapture;
    bool videoIsOpen = false;
    int ret;//return
    int videoFd;//使用的设备
    struct v4l2_capability caps;// 存储摄像头设备能力信息
    struct v4l2_format fmt;// 存储图像格式信息
    struct v4l2_requestbuffers rqbufs;// 用于请求内核分配缓存
    struct v4l2_fmtdesc fmtdesc;// 存储支持的格式描述
    if (ch == 0) {
        videoFd = open("/dev/video0", O_RDWR);
    } else {
        videoFd = open("/dev/video1", O_RDWR);
    }
    memset(&caps, 0, sizeof(caps));
    ret = ioctl(videoFd, VIDIOC_QUERYCAP, &caps);// 查询设备能力，确认设备支持视频采集
    if (ret < 0) {
        qDebug("video%d VIDIOC_QUERYCAP fail %d", ch, ret);
        return;
    }
    fmtdesc.index = 0;// 枚举设备支持的所有格式
    //V4L2_BUF_TYPE_VIDEO_CAPTURE 告诉 V4L2 驱动，接下来要查询的是视频捕获缓冲区支持的像素格式
    //V4L2 驱动支持多种不同类型的操作（捕获CAPTURE、输出OUTPUT、叠加OVERLAY等），每种操作使用不同类型的缓冲区
    fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    while (ioctl(videoFd, VIDIOC_ENUM_FMT, &fmtdesc) != -1) {
        fmtdesc.index++;
    }
    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    // VIDIOC_G_FMT表示获取当前默认的图像格式，那和上面那句是什么区别呢？？
    ret = ioctl(videoFd, VIDIOC_G_FMT, &fmt);
    if (ret < 0) {
        printf("video%d VIDIOC_G_FMT fail %d\n", ch, ret);
    }
    fmt.fmt.pix.width = IMAGEWIDTH;
    fmt.fmt.pix.height = IMAGEHEIGHT;
    fmt.fmt.pix.bytesperline = 2 * fmt.fmt.pix.width;//每个像素占 2 字节
    fmt.fmt.pix.colorspace = V4L2_COLORSPACE_SRGB;//数据的色彩解释方式（如何映射到实际颜色）
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;//数据在内存中的存储方式（编码格式）
    ioctl(videoFd, VIDIOC_S_FMT, &fmt);//VIDIOC_S_FMT 是命令编号，表示 "设置格式"（Set Format）
    // 请求内核分配2个视频缓存
    memset(&rqbufs, 0, sizeof(rqbufs));
    rqbufs.count = IMG_CACHE_MAX;
    rqbufs.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    rqbufs.memory = V4L2_MEMORY_MMAP;//设置缓冲区在DDR中
    ret = ioctl(videoFd, VIDIOC_REQBUFS, &rqbufs);//VIDIOC_REQBUFS 是请求编号，表示 "申请缓冲区"（Request Buffers）
    if (ret < 0) {
        printf("video%d VIDIOC_REQBUFS fail %d\n", ch, ret);
    }
    struct v4l2_buffer buffer;// 定义缓存相关变量
    char *pImagAry[IMG_CACHE_MAX];// pImagAry[i]对应第i个缓冲区的起始地址
    enum v4l2_buf_type type;
    fd_set fds;
    struct timeval tv;
    int r;
    int catCot = 0;
    IplImage* frame_yuv = cvCreateImageHeader(cvSize(IMAGEWIDTH, IMAGEHEIGHT), IPL_DEPTH_8U, 2);
    for (int i = 0; i < IMG_CACHE_MAX; i++) {
        memset(&buffer, 0, sizeof(buffer));
        buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buffer.memory = V4L2_MEMORY_MMAP;
        buffer.index = i;
        if (-1 == ioctl(videoFd, VIDIOC_QUERYBUF, &buffer)) {
            printf("VIDIOC_QUERYBUF fail\n");
            return;
        }
        pImagAry[i] = (char *)mmap(NULL, buffer.length, PROT_READ | PROT_WRITE,
                                  MAP_SHARED, videoFd, buffer.m.offset);
        if (pImagAry[i] == NULL) {
            printf("mmap fail\n");
        }
    }
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ret = ioctl(videoFd, VIDIOC_STREAMON, &type);
    FD_ZERO(&fds);
    FD_SET(videoFd, &fds);
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ret = ioctl(videoFd, VIDIOC_STREAMON, &type);
    tv.tv_sec = 2;
    tv.tv_usec = 0;
    r = select(videoFd + 1, &fds, NULL, NULL, &tv);
    if(r < 0){
        qDebug("none image catch");
    }
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ret = ioctl(videoFd, VIDIOC_STREAMOFF, &type);
    memset(&buffer, 0, sizeof(buffer));
    buffer.type  = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buffer.memory = V4L2_MEMORY_MMAP;
    // 主处理循环（与原代码一致）
    cv::Mat final_image;// 最终处理的图像（BGR格式）
    cv::Mat face_img;// 裁剪出的人脸图像
    cv::Mat jpg_image; // 原始图像的副本（YUV格式
    //Mat eye_roi;
    vector<Point2f> image_points;
    //cv::VideoWriter writer("./tmp/test_face.avi", cv::VideoWriter::fourcc('M', 'J', 'P', 'G'), 5, cv::Size(64, 64));

    //改写这个mode
    int mode = 0;  // 0-摄像头   1-图片
    while (1) {
        static int loop_counter = 0;
        qDebug() << "========== Processing loop" << loop_counter++ << "==========";
        TickMeter cvtm;
        int x, y, w, h;
        int e_x, e_y, e_w, e_h;
        //获取摄像头BUFFER，YUV转为BGR，如果采用MJPEG则不需要
        if (mode == 0) {
            cvtm.start();
            static cv::Mat resized_bgr(IMAGEHEIGHT*SCALE, IMAGEWIDTH*SCALE, CV_8UC3);
            ret = ioctl(videoFd, VIDIOC_STREAMON, &type);
            ioctl(videoFd, VIDIOC_QBUF, &buffer);
            cv::Mat yuyv_frame(IMAGEHEIGHT, IMAGEWIDTH, CV_8UC2, pImagAry[buffer.index]);
            jpg_image = yuyv_frame;
            cv::Mat resized_yuyv;
            cv::resize(yuyv_frame, resized_yuyv,
                      cv::Size(IMAGEWIDTH*SCALE, IMAGEHEIGHT*SCALE),
                      0, 0, cv::INTER_NEAREST);
            cv::cvtColor(resized_yuyv, resized_bgr, cv::COLOR_YUV2BGR_YUYV);


            cv::imwrite("./tmp/resized_bgr.jpg", resized_bgr);
            final_image = resized_bgr;
        }
        // 人脸检测（与原代码一致）
        std:tie(x, y, w, h, e_x, e_y, e_w, e_h, image_points) = libface_detection(final_image);
        cv::Mat face_detected_marked; // 带人脸框+视线的图（和图片输入一致）

        if ((x > 0) && !jpg_image.empty()) {
            qDebug() << "[Face Detection] Valid face detected. x:" << x << "y:" << y << "w:" << w << "h:" << h;

            //新增：克隆final_image（BGR格式），绘制人脸框（和图片输入一致）
            face_detected_marked = final_image.clone();
            cv::rectangle(face_detected_marked, cv::Rect(x, y, w, h), cv::Scalar(0, 255, 0), 2);

            // 原有裁剪face_img的逻辑保留（用于预处理），但不发送它
            face_img = CutOut_Mat(jpg_image, x, y, w, h);
            //resize(face_img, face_img, Size(64, 64));
            // 预处理人脸图像（新增：与Python对应）
            cv::Mat processed_face;
            if (!face_img.empty()) {
                // 1. BGR转RGB
                cv::Mat face_bgr;
                cv::cvtColor(face_img, face_bgr, cv::COLOR_YUV2BGR_YUYV); // YUV→BGR, 摄像头是YUV，图片是BGR，最终都转RGB
                cv::Mat face_rgb;
                cv::cvtColor(face_bgr, face_rgb, cv::COLOR_BGR2RGB);     // BGR→RGB, 统一转为RGB供模型输入
                // 2. 缩放（改为固定448×448，替换原最短边缩放逻辑）
                cv::Mat resized;
                cv::resize(face_rgb, resized, cv::Size(448, 448), 0, 0, cv::INTER_LINEAR);
                // 3. 标准化
                resized.convertTo(processed_face, CV_32FC3, 1.0/255.0);
                vector<cv::Mat> channels(3);
                cv::split(processed_face, channels);
                for (int c = 0; c < 3; c++) {
                    channels[c] = (channels[c] - NORM_MEAN[c]) / NORM_STD[c];
                }
                cv::merge(channels, processed_face);
                // 保存预处理数据（仅图片模式，摄像头模式可选）
                // saveProcessedData(processed_face, "processed_data_cpp.txt");
            }

            // 新增：对摄像头输出的最终图像做水平翻转（纠正镜像）
            cv::Mat flipped_face;
            cv::flip(face_detected_marked, flipped_face, 1); // 参数1：水平翻转
            // 发送图像到UI
            QImage *image = new QImage(cvMatToQImage(flipped_face));
            emit sendImg(image);

            bool binSaveSuccess = saveFaceToBin(processed_face);
            // -------------------------- 对齐图片输入：运行ELF+读取DDR结果 --------------------------
            if (binSaveSuccess) {
                printf("[ELF Run] 摄像头模式：bin文件保存成功，开始执行.elf\n");
                int elfRet = action_elf("./withoutPL_linux.elf");
                if (elfRet == 0) {
                    printf("[ELF Run] ./withoutPL_linux.elf执行完成（无错误）\n");
                    // 与图片输入共用DDR地址（确保一致）
                    unsigned long pitch_addr = 0x00101000;
                    unsigned long yaw_addr = 0x00101004;
                    float read_pitch, read_yaw;
                    // 读取pitch/yaw（与图片输入一致）
                    if (Devmem_Read_Float(pitch_addr, &read_pitch, 1) != 0) {
                        printf("[DDR Read] 摄像头模式：成功读取pitch: %.2f 从地址 0x%lX\n", read_pitch, pitch_addr);
                    } else {
                        printf("[DDR Read] 摄像头模式：错误：读取pitch从地址 0x%lX 失败\n", pitch_addr);
                        read_pitch = 0.0f; // 异常时默认值，避免绘制崩溃
                    }
                    if (Devmem_Read_Float(yaw_addr, &read_yaw, 1) != 0) {
                        printf("[DDR Read] 摄像头模式：成功读取yaw: %.2f 从地址 0x%lX\n", read_yaw, yaw_addr);
                    } else {
                        printf("[DDR Read] 摄像头模式：错误：读取yaw从地址 0x%lX 失败\n", yaw_addr);
                        read_yaw = 0.0f; // 异常时默认值
                    }

                    // 1. 先翻转图像（得到正确显示的图像）
                    cv::Mat flipped_marked;
                    cv::flip(face_detected_marked, flipped_marked, 1);

                    // 2. 计算翻转后的人脸框坐标（水平翻转后x坐标变化）
                    int flipped_x = flipped_marked.cols - x - w;  // 关键：修正x坐标（原x -> 图像宽度 - x - 宽度）
                    int flipped_y = y;  // y坐标不变
                    int flipped_w = w;
                    int flipped_h = h;
                    // 3. 在翻转后的图像上绘制视线（使用修正后的坐标）
                    drawGaze(flipped_marked,  // 绘制到翻转后的图像上
                             flipped_x, flipped_y, flipped_w, flipped_h,  // 翻转后的人脸框坐标
                             -read_pitch, read_yaw);  // 原始计算的pitch/yaw（无需额外调整）

                    // 保存中间结果
                    safe_imwrite("./tmp/1_original_video.jpg", jpg_image);       // 对应图片的1_original_input.jpg
                    safe_imwrite("./tmp/2_scaled_for_detection_video.jpg", final_image); // 对应图片的2_scaled_for_detection.jpg
                    safe_imwrite("./tmp/3_cropped_face_video.jpg", face_img);    // 对应图片的4_cropped_face.jpg
                    safe_imwrite("./tmp/7_final_with_gaze_video.jpg", flipped_marked); // 对应图片的7_final_with_gaze.jpg
                    // 发送图像到UI
                    QImage *gaze_image = new QImage(cvMatToQImage(flipped_marked));
                    emit sendImg(gaze_image);


                } else {
                    printf("[ELF Run] 摄像头模式：./withoutPL.elf 执行失败，返回码：%d\n", elfRet);
                }
            } else {
                printf("[ELF Run] 摄像头模式：bin文件保存失败，跳过 .elf 执行");
            }
            // ------------------------------------------------------------------------

            // 眼睛ROI处理（保留原代码结构，注释无关部分）
            /* eye_roi = extractEyeROI(jpg_image, image_points, model_points, camera_matrix, dist_coeffs, normalized_cam, Size(60,36), this);
            if (mode == 0) {
                Mat bgr_eye_roi;
                cv::cvtColor(eye_roi, bgr_eye_roi, cv::COLOR_YUV2BGR_YUYV);
                cv::cvtColor(bgr_eye_roi, eye_roi, cv::COLOR_BGR2GRAY);
            } else if (mode == 1) {
                cv::cvtColor(eye_roi, eye_roi, cv::COLOR_BGR2GRAY);
            }*/

            cvtm.stop();
            printf("[TIME] gaze estimation cost %gms\n", cvtm.getTimeMilli());

        }
        else{
            // 无人脸时，发送原始检测图（final_image是BGR格式，CV_8UC3，兼容转换）
            QImage *rawImage = new QImage(cvMatToQImage(final_image));
            emit sendImg(rawImage);
            qDebug("[Face Detection] No face detected, send raw frame");
        }
    }
}
void catchImage::processImage()
{
    int x, y, w, h;
    int e_x, e_y, e_w, e_h;
    vector<Point2f> image_points;
    imgPath = "./data.jpg";
    printf("\n进入processImage函数\n");
    printf("当前图片路径：%s\n", imgPath.c_str());
    // 1. 读取图片
    cv::Mat jpg_image = cv::imread(imgPath, cv::IMREAD_COLOR);
    if (jpg_image.empty()) {
        std::cerr << "Error: Could not open image file: " << imgPath << std::endl;
        return;
    }
    cv::imwrite("./tmp/1_original_input.jpg", jpg_image);
    // 2. 缩放图像用于人脸检测（与原代码一致）
    const cv::Size target_size(IMAGEWIDTH * SCALE, IMAGEHEIGHT * SCALE);
    cv::Mat final_image;
    cv::resize(jpg_image, final_image, target_size);
    cv::imwrite("./tmp/2_scaled_for_detection.jpg", final_image);
    // 3. 人脸检测（使用libfacedetection）
    std::tie(x, y, w, h, e_x, e_y, e_w, e_h, image_points) = libface_detection(final_image);
    if (x <= 0) {
        qDebug() << "No face detected in image!";
        return;
    }
    // 在缩放图上标记检测到的人脸区域并保存（中间结果3）
    cv::Mat face_detected_marked = final_image.clone();
    cv::rectangle(face_detected_marked, cv::Rect(x, y, w, h), cv::Scalar(0, 255, 0), 2); // 绿色矩形标记人脸
    cv::imwrite("./tmp/3_face_detected_marked.jpg", face_detected_marked);
    // 4. 裁剪人脸区域（对应Python中y_min:y_max, x_min:x_max）
    // 新增：将检测坐标（基于缩放图final_image）转换为原始图片（jpg_image）的坐标
    // 计算缩放比例：原始图片尺寸 / 缩放后图片尺寸
    float scale_x = static_cast<float>(jpg_image.cols) / final_image.cols;  // 宽度缩放比例
    float scale_y = static_cast<float>(jpg_image.rows) / final_image.rows;  // 高度缩放比例
    // 转换坐标（四舍五入到整数）
    int orig_x = static_cast<int>(round(x * scale_x));       // 原始图上的x
    int orig_y = static_cast<int>(round(y * scale_y));       // 原始图上的y
    int orig_w = static_cast<int>(round(w * scale_x));       // 原始图上的宽度
    int orig_h = static_cast<int>(round(h * scale_y));       // 原始图上的高度
    cv::Mat face_img = CutOut_Mat(jpg_image, orig_x, orig_y, orig_w, orig_h);
    if (face_img.empty()) {
        qDebug() << "Failed to crop face!";
        return;
    }
    cv::imwrite("./tmp/4_cropped_face.jpg", face_img);
    // 5. 预处理（严格对应Python的pre_process函数）
    cv::Mat processed;
    try {
        // 5.1 BGR转RGB（对应cv2.COLOR_BGR2RGB）
        cv::Mat face_rgb;
        cv::cvtColor(face_img, face_rgb, cv::COLOR_BGR2RGB);
        // 保存RGB转换后的图片
        //（注意：OpenCV显示默认BGR，这里转BGR后保存才能正常显示）（中间结果5）
        cv::Mat face_rgb_bgr;
        cv::cvtColor(face_rgb, face_rgb_bgr, cv::COLOR_RGB2BGR); // 转回BGR用于正确保存
        cv::imwrite("./tmp/5_face_rgb.jpg", face_rgb_bgr);
        // 5.2 最短边缩放到448（对应transforms.Resize(448)）
        cv::Mat resized;
        cv::resize(face_rgb, resized, cv::Size(448, 448), 0, 0, cv::INTER_LINEAR);
        // 保存缩放后的预处理图片（同样转BGR保存）（中间结果6）
        cv::Mat resized_bgr;
        cv::cvtColor(resized, resized_bgr, cv::COLOR_RGB2BGR);
        cv::imwrite("./tmp/6_resized_preprocessed_448_448.jpg", resized_bgr);
        // 5.3 标准化（对应ToTensor()，标准化后是浮点数，不适合直接保存为图片）
        resized.convertTo(processed, CV_32FC3, 1.0/255.0);
        vector<cv::Mat> channels(3);
        cv::split(processed, channels);
        for (int c = 0; c < 3; ++c) {
            channels[c] = (channels[c] - NORM_MEAN[c]) / NORM_STD[c];
        }
        cv::merge(channels, processed);

        qDebug() << QString("after processed img size (C, H, W): (3, %1, %2)").arg(processed.rows).arg(processed.cols);
        qDebug() << QString("with batch img size (N, C, H, W): (1, 3, %1, %2)").arg(processed.rows).arg(processed.cols);
    } catch (const cv::Exception& e) {
        qDebug() << "预处理失败: " << e.what();
        return;
    }
    // -------------------------- 新增：调用bin保存函数 --------------------------
    bool binSaveSuccess =saveFaceToBin(processed); // processed是预处理后448×448、CV_32FC3的人脸图像
    // ------------------------------------------------------------------------

    // -------------------------- 新增：运行 suan.elf（bin保存成功后） --------------------------
    if (binSaveSuccess) {
        printf("[ELF Run] bin文件保存成功，开始执行.elf\n");
        int elfRet = action_elf("./withoutPL_linux.elf"); // 调用现有action_elf函数执行elf
        if (elfRet == 0) {
               printf("[ELF Run] ./withoutPL_linux.elf 执行完成（无错误）\n");
               // 定义要读取的DDR地址 (确保4字节对齐)
               unsigned long pitch_addr = 0x00101000;  // 4字节对齐地址
               unsigned long yaw_addr = 0x00101004;    // 4字节对齐地址
               // 读取pitch和yaw值
               float read_pitch, read_yaw;
               // 读取pitch值
               if (Devmem_Read_Float(pitch_addr, &read_pitch, 1) != 0) {
                   printf("[DDR Read] 成功读取pitch: %.2f 从地址 0x%lX\n", read_pitch, pitch_addr);
               } else {
                   printf("[DDR Read] 错误：读取pitch从地址 0x%lX 失败\n", pitch_addr);
               }
               // 读取yaw值
               if (Devmem_Read_Float(yaw_addr, &read_yaw, 1) != 0) {
                   printf("[DDR Read] 成功读取yaw: %.2f 从地址 0x%lX\n", read_yaw, yaw_addr);
               } else {
                   printf("[DDR Read] 错误：读取yaw从地址 0x%lX 失败\n", yaw_addr);
               }
               drawGaze(face_detected_marked,  // 绘制目标图像（已有人脸框标记）
                            x, y, w, h,            // 人脸框坐标（从libface_detection获取）
                            read_pitch, -read_yaw); // 直接传入弧度制角度
               imwrite("./tmp/7_final_with_gaze.jpg", face_detected_marked);
           } else {
               printf("[ELF Run] ./withoutPL.elf 执行失败，返回码：%d\n", elfRet);
           }
    } else {
       printf("[ELF Run] bin文件保存失败，跳过 .elf 执行");
    }
    // ------------------------------------------------------------------------

    // 6. 保存预处理数据（原代码已有，略）
    /*
    std::ofstream fout("./tmp/processed_data_cpp.txt");
    if (fout.is_open()) {
        // 展平数据（C×H×W顺序，与Python张量展平一致）
        int channels = processed.channels();
        int rows = processed.rows;
        int cols = processed.cols;
        for (int c = 0; c < channels; ++c) {
            for (int i = 0; i < rows; ++i) {
                const float* row_ptr = processed.ptr<float>(i);
                for (int j = 0; j < cols; ++j) {
                    // 按C×H×W顺序写入（与PyTorch张量内存布局一致）
                    fout << std::fixed << std::setprecision(8)
                         << row_ptr[j * channels + c] << "\n";
                }
            }
        }
        fout.close();
        int total = channels * rows * cols;
        qDebug() << QString("processed data been saved in: processed_data_cpp.txt>total num:%1 ")
                    .arg(total);
    } else {
        qDebug() << "can't open file and saved processed data";
    }
    */
    //printf("[程序结束] 图片处理及ELF执行完成，准备退出...\n");
    //QApplicati  on::quit();  // 通知Qt事件循环退出
    // 7. 发送人脸图像到UI显示（原代码不变）
    //QImage *qimg = new QImage(cvMatToQImage(face_img));

    QImage *qimg = new QImage(cvMatToQImage(face_detected_marked));
    emit sendImg(qimg);
}

// 人脸检测函数（与原代码完全一致）
std::tuple<int, int, int, int, int, int, int, int, vector<Point2f>> libface_detection(Mat input_img)
{
    Mat image = input_img.clone();
    int x = -2, y = -2, w = -2, h = -2;
    float e_x = -3, e_y = -3, e_w = -3, e_h = -3;
    int *pResults = NULL;
    vector<Point2f> image_points;

    pResults = facedetect_cnn(pBuffer, (unsigned char*)(image.ptr(0)), image.cols, image.rows, (int)image.step);
    printf("[libface_detection]detected %d faces .\n", (pResults ? *pResults : 0));

    /*if (pResults) {
        for (int i = 0; i < (pResults ? *pResults : 0); i++) {*/
    if (pResults && *pResults > 0) {  // 仅处理第一个检测到的人脸
            // 只取第一个人脸（i=0），忽略后续所有
            short *p = ((short*)(pResults + 1)) + 16 * 0;
            int confidence = p[0];
            if (confidence >= 50) {
            x = p[1];
            y = p[2];
            w = p[3];
            h = p[4];
            e_x = p[7];
            e_y = p[8];
            image_points = {
                Point2f(p[5]*CUT_THRESHOLD, p[6]*CUT_THRESHOLD),
                Point2f(p[7]*CUT_THRESHOLD, p[8]*CUT_THRESHOLD),
                Point2f(p[9]*CUT_THRESHOLD, p[10]*CUT_THRESHOLD),
                Point2f(p[11]*CUT_THRESHOLD, p[12]*CUT_THRESHOLD),
                Point2f(p[13]*CUT_THRESHOLD, p[14]*CUT_THRESHOLD)
            };
       }
        }else {
        return {0, 0, 0, 0, 0, 0, 0, 0, {}};
    }
    // 计算眼睛区域（与原代码一致）
    e_h = h / EYE_SCALE_Y;
    e_w = w / EYE_SCALE_X;
    e_x = e_x - e_w / 2;
    e_y = e_y - e_h / 2;
    return {x, y, w, h, (int)e_x, (int)e_y, (int)e_h, (int)e_w, image_points};
}



// 其他辅助函数（与原代码完全一致）
void safe_imwrite(const std::string& path, cv::Mat img)
{
    if (img.channels() == 2) {
        cv::Mat tmp;
        cv::cvtColor(img, tmp, cv::COLOR_YUV2BGR_YUYV);
        cv::imwrite(path, tmp);
    } else {
        cv::imwrite(path, img);
    }
}

/**
 * 功能：将448×448的CV_32FC3格式人脸（RGB通道）转为NCHW格式并保存为bin文件
 * 输入：inputMat - 预处理后的人脸（必须是448×448、CV_32FC3、RGB顺序）
 *       savePath - 保存路径（固定为./FaceResult/img.bin）
 */
bool saveFaceToBin(const cv::Mat& inputMat) {
    const std::string savePath = "./FaceResult/img.bin"; // 固定目标路径
    // 核心参数检查（避免无效数据）
    if (inputMat.empty()) {
        printf("[Bin Save] 错误：输入图像为空！\n");
        return false;
    }
    if (inputMat.type() != CV_32FC3) {
        printf("[Bin Save] 错误：图像类型必须是CV_32FC3！\n");
        return false;
    }
    if (inputMat.rows != 448 || inputMat.cols != 448) {
        printf("[Bin Save] 错误：图像尺寸必须是448×448！\n");
        return false;
    }
    // HWC（448×448×3）转NCHW（1×3×448×448），与Python格式对齐
    const int N = 1, C = 3, H = 448, W = 448;
    std::vector<float> nchwData(N * C * H * W); // 存储转换后的数据
    size_t dataIdx = 0;
    // 按 N→C→H→W 顺序填充（通道优先：先R、再G、最后B）
    for (int c = 0; c < C; ++c) {
        for (int h = 0; h < H; ++h) {
            const cv::Vec3f* rowPtr = inputMat.ptr<cv::Vec3f>(h); // 每行数据指针
            for (int w = 0; w < W; ++w) {
                nchwData[dataIdx++] = rowPtr[w][c]; // 提取当前通道像素
            }
        }
    }
    // 二进制模式写入bin文件（float32格式，与Python numpy.tofile一致）
    std::ofstream binFile(savePath, std::ios::binary | std::ios::out);
    if (!binFile.is_open()) {
        printf("[Bin Save] 错误：无法打开文件 %s\n", savePath.c_str());
        return false;
    }
    // 写入全部float32数据（直接拷贝内存）
    binFile.write(reinterpret_cast<const char*>(nchwData.data()), nchwData.size() * sizeof(float));
    binFile.close();
    // 日志提示
    printf("[Bin Save] 成功！文件已保存至：%s\n", savePath.c_str());
    printf("[Bin Save] 文件大小：%zu 字节（1×3×448×448 float32）\n", nchwData.size() * sizeof(float));
    return true;
}

IplImage *CutOut(IplImage *source_image, int x, int y, int w, int h)
{
    cv::Rect roi;
    roi.x = x * CUT_THRESHOLD;
    roi.y = y * CUT_THRESHOLD;
    roi.width = w * CUT_THRESHOLD;
    roi.height = h * CUT_THRESHOLD;
    IplImage *src_resize = cvCreateImage(cvSize(roi.width, roi.height), source_image->depth, source_image->nChannels);
    cvSetImageROI(source_image, roi);
    cvResize(source_image, src_resize, CV_INTER_NN);
    cvResetImageROI(source_image);
    return src_resize;
}

cv::Mat CutOut_Mat(const cv::Mat& source_image, int x, int y, int w, int h)
{
    if (source_image.empty()) {
        std::cerr << "Error: Source image is empty!" << std::endl;
        return cv::Mat();
    }

    // 直接使用传入的坐标（已转换为原始图坐标），无需再乘以CUT_THRESHOLD
    cv::Rect roi_rect(x, y, w, h);
    // 确保ROI在图片范围内（防止越界）
    cv::Rect valid_roi = roi_rect & cv::Rect(0, 0, source_image.cols, source_image.rows);
    if (valid_roi.width <= 0 || valid_roi.height <= 0) {
        std::cerr << "Error: Invalid ROI dimensions! ROI: " << roi_rect
                  << " Image size: " << source_image.size() << std::endl;
        return cv::Mat();
    }

    // 直接返回裁剪区域（克隆避免引用原始图片）
    return source_image(valid_roi).clone();
}

QImage cvMatToQImage(const cv::Mat& mat)
{
    if (mat.empty() || mat.data == nullptr) {
        qDebug("[Face Detection]no face return empty QImage");
        return QImage();
    }

    switch (mat.type()) {
        case CV_8UC1: {
            QImage image(mat.cols, mat.rows, QImage::Format_Indexed8);
            image.setColorCount(256);
            for (int i = 0; i < 256; i++) {
                image.setColor(i, qRgb(i, i, i));
            }
            uchar *pSrc = mat.data;
            for (int row = 0; row < mat.rows; row++) {
                uchar *pDest = image.scanLine(row);
                memcpy(pDest, pSrc, mat.cols);
                pSrc += mat.step;
            }
            return image;
        }
        case CV_8UC3: {
            const uchar *pSrc = (const uchar*)mat.data;
            QImage image(pSrc, mat.cols, mat.rows, mat.step, QImage::Format_RGB888);
            return image.rgbSwapped();
        }
        case CV_8UC4: {
            const uchar *pSrc = (const uchar*)mat.data;
            QImage image(pSrc, mat.cols, mat.rows, mat.step, QImage::Format_ARGB32);
            return image.copy();
        }
        default:
            break;
    }
    return QImage();
}

int Devmem_Read(unsigned long readAddr, unsigned long* buf, unsigned long len, int status)
{
    int i = 0;
    int fd;
    void *map_base, *virt_addr;
    off_t addr = readAddr;
    unsigned long page_addr = addr & ~MAP_MASK;

    if ((fd = open("/dev/mem", O_RDWR | O_SYNC)) == -1) {
        fprintf(stderr, "Error at line %d, file %s (%d) [%s]\n", __LINE__, __FILE__, errno, strerror(errno));
        return 0;
    }

    map_base = mmap(0, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, addr & ~MAP_MASK);
    if (map_base == (void *) -1) {
        fprintf(stderr, "Error at line %d, file %s (%d) [%s]\n", __LINE__, __FILE__, errno, strerror(errno));
        close(fd);
        return 0;
    }

    for (i = 0; i < len; i++) {
        if ((addr & ~MAP_MASK) != page_addr) {
            page_addr = addr & ~MAP_MASK;
            if (munmap(map_base, MAP_SIZE) == -1) {
                fprintf(stderr, "Error at line %d, file %s (%d) [%s]\n", __LINE__, __FILE__, errno, strerror(errno));
                close(fd);
                return 0;
            }
            map_base = mmap(0, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, addr & ~MAP_MASK);
            if (map_base == (void *) -1) {
                fprintf(stderr, "Error at line %d, file %s (%d) [%s]\n", __LINE__, __FILE__, errno, strerror(errno));
                close(fd);
                return 0;
            }
        }

        virt_addr = map_base + (addr & MAP_MASK);
        if (status == 0) {
            buf[i] = *((unsigned long *) virt_addr);
        }
        if (status == 1) {
            *((unsigned long *)virt_addr) = buf[i];
        }
        addr += 4;
    }

    if (munmap(map_base, MAP_SIZE) == -1) {
        fprintf(stderr, "Error at line %d, file %s (%d) [%s]\n", __LINE__, __FILE__, errno, strerror(errno));
        close(fd);
        return 0;
    }
    close(fd);
    return i;
}

unsigned long get_register(unsigned long base_addr)
{
    unsigned long buffer[2];
    Devmem_Read(base_addr, buffer, 1, 0);
    return buffer[0];
}

void write_register(unsigned long base_addr, unsigned long data)
{
    unsigned long buffer[2];
    buffer[0] = data;
    Devmem_Read(base_addr, buffer, 1, 1);
}


int action_elf(string command)
{
    //system 是 C 标准库（stdlib.h）中提供的一个函数，作用是 调用操作系统的命令行解释器（shell）执行一个字符串命令。
    //等价于在终端输入 ./withoutPL.elf 并执行
    int result = system(command.c_str());
    if (result == -1) {
        std::cerr << "system() failed: " << strerror(errno) << std::endl;
        return -1; // system调用本身失败（如资源不足）
    }
    if (WIFEXITED(result)) {//正常退出，返回非 0（真）
        int exit_status = WEXITSTATUS(result);// 获取ELF的退出码,一切正常时为0
        std::cout << ".elf execution exited with status: " << exit_status << std::endl;
        return exit_status; // 返回ELF的退出码（0=成功，非0=失败）
    } else {
        std::cout << ".elf was interrupted or crashed" << std::endl;
        return -2; // ELF未正常退出（如被信号中断）
    }
}

IplImage* input_img()
{
    IplImage* input_image = cvLoadImage("./face.jpg", CV_LOAD_IMAGE_COLOR);
    if (!input_image || !input_image->imageData) {
        fprintf(stderr, "Failed to load image\n");
        exit(1);
    }
    IplImage* frame_yuv = cvCreateImage(cvSize(IMAGEWIDTH, IMAGEHEIGHT), IPL_DEPTH_8U, 2);
    CvSize target_size = cvSize(64, 48);
    IplImage *outImage = cvCreateImage(target_size, frame_yuv->depth, frame_yuv->nChannels);
    cvResize(frame_yuv, outImage);
    return outImage;
}

int16_t float_to_int16(float value, int n)
{
    if (value == 1) {
        return 32767;
    } else {
        float scaled = value * (1 << n);
        return static_cast<int16_t>(round(scaled));
    }
}


Mat convert_normalized_to_fixedpoint(cv::Mat& src, int n)
{
    cv::Mat dst(src.rows, src.cols, CV_16SC1);
    for (int i = 0; i < src.rows; ++i) {
        const float* src_row = src.ptr<float>(i);
        int16_t* dst_row = dst.ptr<int16_t>(i);
        for (int j = 0; j < src.cols; ++j) {
            dst_row[j] = float_to_int16(src_row[j], n);
        }
    }
    return dst;
}

int16_t read_int16_from_addr(unsigned long addr)
{
    int16_t value = 0;
    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd < 0) {
        qDebug() << "Failed to open /dev/mem";
        return 0;
    }

    unsigned long page_addr = addr & ~0xFFF;
    void* map_base = mmap(0, 4096, PROT_READ, MAP_SHARED, fd, page_addr);
    if (map_base == MAP_FAILED) {
        close(fd);
        qDebug() << "mmap failed";
        return 0;
    }

    unsigned long offset = addr & 0xFFF;
    value = *(reinterpret_cast<int16_t*>(map_base + offset));
    munmap(map_base, 4096);
    close(fd);
    return value;
}

int check_bit(unsigned long base_addr, unsigned long bit, int num)
{
    if (bit > 31 || (num != 0 && num != 1)) {
        return -1;
    }
    unsigned long value = get_register(base_addr);
    int bit_value = (value & (1UL << bit)) ? 1 : 0;
    return (bit_value == num);
}

int set_bit(unsigned long base_addr, unsigned long bit, int num)
{
    unsigned long value = get_register(base_addr);
    if (num) {
        value |= (1UL << bit);
    } else {
        value &= ~(1UL << bit);
    }
    write_register(base_addr, value);
    return 0;
}

void catchImage::sendResults(int16_t result0, int16_t result1)
{
#pragma pack(push, 1)
    struct ResultsData {
        int16_t res0;
        int16_t res1;
    } data;
#pragma pack(pop)

    static_assert(sizeof(data) == 4, "ResultsData size mismatch");
    data.res0 = result0;
    data.res1 = result1;

    ssize_t sent = sendto(sockfd, &data, sizeof(data), MSG_CONFIRM,
                         (const struct sockaddr*)&servaddr, sizeof(servaddr));
    if (sent != sizeof(data)) {
        qDebug() << "---------UDP send failed:" << strerror(errno);
    }

    qDebug() << "[NET]Sent results:"
             << "result0=" << result0
             << "result1=" << result1;
}

// run_elf类实现
run_elf::run_elf() : QThread() {}

void run_elf::run()
{
    //std::string command = "./linux_ov5640.elf";
    std::string command = "./withoutPL.elf";
    action_elf(command);
    while (true) {
        continue;
    }
}
#include <cmath>
#include <opencv2/opencv.hpp>
using namespace cv;

/**
 * 视线箭头绘制函数（已适配弧度制，直接使用read_pitch/read_yaw）
 * @param img：绘制目标图像（如face_detected_marked）
 * @param bbox_x/y/w/h：人脸框坐标（与img坐标系一致）
 * @param pitch_rad：弧度制俯仰角（直接从DDR读取）
 * @param yaw_rad：弧度制偏航角（直接从DDR读取）
 * @param color：箭头颜色（默认红色）
 * @param thickness：箭头线宽（默认2）
 */
void drawGaze(Mat& img, int bbox_x, int bbox_y, int bbox_w, int bbox_h,
              float pitch_rad, float yaw_rad,
              Scalar color, int thickness ) {
    // 1. 计算人脸框中心点（箭头起点）
    int center_x = bbox_x + bbox_w / 2;
    int center_y = bbox_y + bbox_h / 2;

    // 2. 箭头长度：与人脸框宽度成正比（协调美观）
    int arrow_length = bbox_w;  // 可微调（如bbox_w*0.8，根据显示效果调整）

    // 3. 计算箭头终点（核心公式，基于弧度制，与Python完全一致）
    int dx = static_cast<int>(-arrow_length * sin(pitch_rad) * cos(yaw_rad));
    int dy = static_cast<int>(-arrow_length * sin(yaw_rad));

    // 4. 绘制：人脸中心实心圆 + 视线箭头（抗锯齿）
    circle(img, Point(center_x, center_y), 4, color, -1);  // 中心标记
    arrowedLine(img,
               Point(center_x, center_y),  // 起点
               Point(center_x + dx, center_y + dy),  // 终点
               color, thickness, LINE_AA, 0, 0.25);  // 抗锯齿+箭头尖端长度0.25
}
