#include <stdio.h>
#include <opencv2/opencv.hpp>
#include <opencv2/highgui/highgui.hpp>
#include "facedetectcnn.h"
#include "GazeWorker.h"
#include <linux/fb.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include "get_eye_roi.h"
#include <thread>      // 添加线程支持
#include <atomic>      // 添加原子变量支持
#include <csignal>     // 添加信号处理支持
#include <sys/wait.h>  // 添加进程等待支持
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <vector>
#include <mutex>
#include <chrono>
#include <turbojpeg.h>
#include <vector>
#include <cstring>
#include <arpa/inet.h>
#define DETECT_BUFFER_SIZE 0x9000
#define CUT_THRESHOLD 8

using namespace cv;
using namespace std;

// 全局变量用于ELF进程管理
std::atomic<bool> g_running{true};  // 控制程序运行
pid_t g_elf_pid = 0;                // ELF进程ID
std::atomic<bool> g_elf_ready{false}; // ELF就绪标志

// volatile bool g_running = true;
cv::VideoCapture global_cap;
std::mutex global_frame_mutex;
cv::Mat global_current_frame;
std::atomic<bool> global_frame_ready{false};
std::thread global_capture_thread;

// 摄像头捕获线程函数
void cameraCaptureThread(int camera_index) {
    cv::VideoCapture cap(camera_index);
    if (!cap.isOpened()) {
        std::cerr << "[Camera] 无法打开摄像头" << std::endl;
        return;
    }
    
    // 设置参数
    cap.set(cv::CAP_PROP_FRAME_WIDTH, 640);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, 480);
    cap.set(cv::CAP_PROP_FPS, 30);
    cap.set(cv::CAP_PROP_BUFFERSIZE, 1);
    
    std::cout << "[Camera] 摄像头启动: " 
              << cap.get(cv::CAP_PROP_FRAME_WIDTH) << "x"
              << cap.get(cv::CAP_PROP_FRAME_HEIGHT) << " @ "
              << cap.get(cv::CAP_PROP_FPS) << "fps" << std::endl;
    
    cv::Mat frame;
    int frame_count = 0;
    auto last_time = std::chrono::steady_clock::now();
    
    while (g_running) {
        // 读取帧
        if (cap.read(frame) && !frame.empty()) {
            frame_count++;
            
            // 更新全局帧
            {
                std::lock_guard<std::mutex> lock(global_frame_mutex);
                frame.copyTo(global_current_frame);
                global_frame_ready = true;
            }
            
            // 每30帧打印统计
            if (frame_count % 30 == 0) {
                auto now = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_time);
                double fps = 30.0 * 1000.0 / elapsed.count();
                std::cout << "[Camera] 捕获FPS: " << fps << std::endl;
                last_time = now;
            }
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        
        // 短暂休眠避免CPU占用过高
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
    
    cap.release();
    std::cout << "[Camera] 捕获线程结束" << std::endl;
}



// 定义数据结构
#pragma pack(push, 1)  // 1字节对齐，避免结构体填充
struct FaceGazeData {
    uint32_t frame_id;      // 帧ID
    uint32_t data_size;     // 图像数据大小
    uint32_t face_x;        // 人脸框x
    uint32_t face_y;        // 人脸框y
    uint32_t face_w;        // 人脸框宽度
    uint32_t face_h;        // 人脸框高度
    int32_t pitch;           // 俯仰角
    int32_t yaw;            // 偏航角
    uint32_t has_rot;       // 是否有旋转矩阵：0-无，1-有
    uint32_t rot_size;      // 旋转矩阵数据大小（字节数）
    int32_t    eye_center_x;
    int32_t    eye_center_y;
    int32_t    eye_center_z;
};
#pragma pack(pop)
// 联合体用于浮点数到整型的转换
union FloatUint32 {
    float f;
    uint32_t i;
};


// UDP相关全局变量
int sockfd = -1;
struct sockaddr_in dest_addr;
bool udp_initialized = false;
int udp_port = 8888;  // UDP端口号
char udp_target_ip[16] = "192.168.100.3";  // 电脑IP地址，需要根据实际情况修改

// 初始化UDP传输
bool init_udp_transmission(const char* target_ip, int port) {
    // 创建UDP socket
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("UDP socket creation failed");
        return false;
    }
    
    // 设置socket选项，允许广播
    int broadcast = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) < 0) {
        perror("UDP socket option failed");
        close(sockfd);
        return false;
    }
    
    // 设置发送超时（可选）
    struct timeval tv;
    tv.tv_sec = 1;  // 1秒超时
    tv.tv_usec = 0;
    if (setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) < 0) {
        perror("UDP set timeout failed");
        // 继续执行，这不是致命错误
    }
    
    // 配置目标地址
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, target_ip, &dest_addr.sin_addr) <= 0) {
        cerr << "[Network] Invalid target IP address: " << target_ip << endl;
        close(sockfd);
        return false;
    }
    
    strncpy(udp_target_ip, target_ip, sizeof(udp_target_ip)-1);
    udp_port = port;
    udp_initialized = true;
    
    cout << "[Network] UDP initialized, sending to " << target_ip << ":" << port << endl;
    return true;
}


// 发送函数
bool send_face_gaze_data(const cv::Mat& image, 
                         int face_x, int face_y, int face_w, int face_h,
                         float pitch, float yaw, 
                         int quality = 80) {
    if (!udp_initialized || sockfd < 0) {
        return false;
    }
    
    static int frame_counter = 0;
    frame_counter++;
    
    // 1. 编码图像为JPEG
    std::vector<uchar> buffer;
    std::vector<int> params = {cv::IMWRITE_JPEG_QUALITY, quality};
    
    if (!cv::imencode(".jpg", image, buffer, params)) {
        cerr << "[Network] Failed to encode image to JPEG" << endl;
        return false;
    }
    
    int image_size = buffer.size();
    
    // 2. 准备数据结构
    FaceGazeData data;
    data.frame_id = htonl(frame_counter);
    data.data_size = htonl(image_size);
    data.face_x = htonl(face_x);
    data.face_y = htonl(face_y);
    data.face_w = htonl(face_w);
    data.face_h = htonl(face_h);
    
    // 浮点数转换（需要特殊处理）
    union {
        float f;
        uint32_t i;
    } pitch_union, yaw_union;
    
    pitch_union.f = pitch;
    yaw_union.f = yaw;
    data.pitch = pitch_union.f;  // 直接发送，假设小端序
    data.yaw = yaw_union.f;      // 如果架构不同需要转换
    
    // 3. 创建完整数据包
    std::vector<uchar> packet;
    packet.resize(sizeof(FaceGazeData) + image_size);
    
    // 复制数据结构
    memcpy(packet.data(), &data, sizeof(FaceGazeData));
    
    // 复制图像数据
    memcpy(packet.data() + sizeof(FaceGazeData), buffer.data(), image_size);
    
    // 4. 发送
    ssize_t sent = sendto(sockfd, packet.data(), packet.size(), 0,
                         (struct sockaddr*)&dest_addr, sizeof(dest_addr));
    
    if (sent != packet.size()) {
        cerr << "[Network] Failed to send face gaze data" << endl;
        return false;
    }
    
    if (frame_counter % 30 == 0) {
        cout << "[Network] Sent frame " << frame_counter 
             << ", face: " << face_x << "," << face_y << " " << face_w << "x" << face_h
             << ", gaze: pitch=" << pitch << ", yaw=" << yaw
             << ", total size: " << packet.size() << " bytes" << endl;
    }
    
    return true;
}

//SEND YUV
// 发送YUV420数据（比BGR小1/3）
#include <turbojpeg.h>  // TurboJPEG头文件

// 使用TurboJPEG编码的函数
#include <chrono>

bool send_turbojpeg_fixed(const cv::Mat& normalizing_rot,const cv::Mat& bgr_frame, 
                         int face_x, int face_y, int face_w, int face_h,
                         int pitch, int yaw,float eye_center_x,float eye_center_y,float eye_center_z) {
    if (!udp_initialized || sockfd < 0) {
        return false;
    }
    
    static int frame_counter = 0;
    frame_counter++;
    
    cout << "[TurboJPEG] 开始发送帧 " << frame_counter << endl;
    
    // 1. 确保图像大小合适
    cv::Mat resized_frame = bgr_frame;
    
    // 如果图像太大，先缩小
    if (bgr_frame.cols > 640 || bgr_frame.rows > 480) {
        cv::resize(bgr_frame, resized_frame, cv::Size(640, 480));
        cout << "[TurboJPEG] 缩小图像到: 640x480" << endl;
    }
    
    // 2. 使用TurboJPEG压缩
    tjhandle tjInstance = tjInitCompress();
    if (!tjInstance) {
        cerr << "[TurboJPEG] 初始化失败" << endl;
        return false;
    }
    
    unsigned char* jpegBuf = NULL;
    unsigned long jpegSize = 0;
    
    int result = tjCompress2(tjInstance,
                            resized_frame.data,
                            resized_frame.cols,
                            0,
                            resized_frame.rows,
                            TJPF_BGR,
                            &jpegBuf,
                            &jpegSize,
                            TJSAMP_420,
                            50,  // 质量
                            0);
    
    if (result != 0) {
        cerr << "[TurboJPEG] 压缩失败" << endl;
        tjDestroy(tjInstance);
        return false;
    }
    
    cout << "[TurboJPEG] 压缩后大小: " << jpegSize << " 字节" << endl;
    
    // // 3. 准备头部（使用原来的32字节头部）
    // struct FaceGazeData {
    //     uint32_t frame_id;      // 4字节
    //     uint32_t data_size;     // 4字节
    //     uint32_t face_x;        // 4字节
    //     uint32_t face_y;        // 4字节
    //     uint32_t face_w;        // 4字节
    //     uint32_t face_h;        // 4字节
    //     float pitch;           // 4字节
    //     float yaw;            // 4字节
    // } header;
    // 处理旋转矩阵
    FaceGazeData header;
    std::vector<float> rot_matrix_data;
    uint32_t rot_data_size = 0;
    
    if (!normalizing_rot.empty() && 
        normalizing_rot.rows == 3 && 
        normalizing_rot.cols == 3) {
        
        // 确保矩阵是浮点类型
        cv::Mat rot_mat_float;
        if (normalizing_rot.type() != CV_32F) {
            normalizing_rot.convertTo(rot_mat_float, CV_32F);
        } else {
            rot_mat_float = normalizing_rot;
        }
        
        // 获取矩阵数据（行优先）
        rot_matrix_data.resize(9);  // 3x3 = 9个元素
        const float* data_ptr = rot_mat_float.ptr<float>();
        for (int i = 0; i < 9; i++) {
            rot_matrix_data[i] = data_ptr[i];
        }
        
        header.has_rot = htonl(1);
        header.rot_size = htonl(9 * sizeof(float));  // 9个float，每个4字节
        rot_data_size = 9 * sizeof(float);
    } else {
        header.has_rot = htonl(0);
        header.rot_size = htonl(0);
    }

    
    header.frame_id = htonl(frame_counter);
    header.data_size = htonl(static_cast<uint32_t>(jpegSize)+static_cast<uint32_t>(rot_data_size));
    header.face_x = htonl(face_x);
    header.face_y = htonl(face_y);
    header.face_w = htonl(face_w);
    header.face_h = htonl(face_h);
    header.pitch  = htonl(pitch);
    header.yaw    = htonl(yaw); 
    cout << "pitch" << pitch << " yaw" << yaw << endl;   
    // 浮点数转换
    // union {
    //     float f;
    //     int32_t i;
    // } x, y,z;
    
    int32_t x = float_to_int16(eye_center_x, 15);
    int32_t y = float_to_int16(eye_center_y, 15);
    int32_t z = float_to_int16(eye_center_z, 15);
    cout << "eye_center_x" << x << " eye_center_y" << y <<" eye_center_z" << z << endl;   
    header.eye_center_x = htonl(x);
    header.eye_center_y = htonl(y);
    header.eye_center_z = htonl(z);

    // 计算总大小
    int header_size = sizeof(FaceGazeData);
    int total_size = header_size + jpegSize + rot_data_size;    
    // 4. 创建完整数据包，一次性发送
    // int header_size = sizeof(FaceGazeData);
    // int total_size = header_size + jpegSize;
    
    cout << "[TurboJPEG] 总包大小: " << total_size << " 字节" << endl;
    
    // 检查是否超过UDP限制
    if (total_size > 65507) {
        cerr << "[错误] 数据包太大: " << total_size << " 字节" << endl;
        tjFree(jpegBuf);
        tjDestroy(tjInstance);
        return false;
    }
    
    // 创建完整数据包
    std::vector<uchar> packet(total_size);
    memcpy(packet.data(), &header, header_size);

    // 2. 复制旋转矩阵数据（如果有）
    if (header.has_rot && rot_data_size > 0) {
        // 将每个float转换为网络字节序
        for (size_t i = 0; i < rot_matrix_data.size(); i++) {
            // FloatUint32 fu;
            // fu.f = rot_matrix_data[i];
            int32_t fu = float_to_int16(rot_matrix_data[i], 15);
            int32_t net_value = htonl(fu);
            memcpy(packet.data() + header_size+ i*sizeof(uint32_t), &net_value, sizeof(uint32_t));
        }
    }

    memcpy(packet.data() + header_size + rot_data_size, jpegBuf, jpegSize);
    
    // 5. 一次性发送
    ssize_t sent = sendto(sockfd, packet.data(), packet.size(), 0,
                         (struct sockaddr*)&dest_addr, sizeof(dest_addr));
    
    // 6. 清理
    tjFree(jpegBuf);
    tjDestroy(tjInstance);
    
    bool success = (sent == total_size);
    
    if (success) {
        cout << "[TurboJPEG] ✓ 帧 " << frame_counter 
             << " 发送成功, 大小: " << total_size << " 字节" << endl;
    } else {
        cerr << "[TurboJPEG] ✗ 发送失败: 期望 " << total_size 
             << " 字节, 实际 " << sent << " 字节" << endl;
    }
    
    return success;
}
// 清理UDP资源
void cleanup_udp() {
    if (sockfd >= 0) {
        close(sockfd);
        sockfd = -1;
    }
    udp_initialized = false;
    cout << "[Network] UDP resources cleaned up" << endl;
}

// 信号处理函数
void signal_handler(int signal) {
    cout << "\n[Main] Received signal " << signal << ", shutting down..." << endl;
    g_running = false;
    
    // 停止ELF进程
    if (g_elf_pid > 0) {
        kill(g_elf_pid, SIGTERM);
    }
    exit(0);
}

// ELF守护进程线程函数
void elf_daemon_thread() {
    cout << "[ELF Thread] Starting ELF daemon thread..." << endl;
    
    while (g_running) {
        // 创建子进程运行ELF
        pid_t pid = fork();
        
        if (pid == 0) {
            // 绑定ELF子进程到CPU1
            // cpu_set_t cpuset;
            // CPU_ZERO(&cpuset);
            // CPU_SET(1, &cpuset);
            // sched_setaffinity(0, sizeof(cpu_set_t), &cpuset);
            
            // 设置最低优先级
            nice(19);
            cout << "[ELF Process] ELF daemon thread priority to 19" << endl;
            // 子进程：运行ELF程序
            cout << "[ELF Process] Starting linux_ov5640.elf..." << endl;
            
            // 执行ELF程序
            execl("./linux_ov5640.elf", "./linux_ov5640.elf", NULL);
            
            // 如果execl失败
            perror("[ELF Process] execl failed");
            exit(1);
        } 
        else if (pid > 0) {
            // 父进程（当前线程）
            g_elf_pid = pid;
            cout << "[ELF Thread] ELF process started, PID: " << pid << endl;
            
            // 等待ELF进程初始化
            cout << "[ELF Thread] Waiting for ELF initialization..." << endl;
            sleep(2);  // 等待2秒，让ELF初始化
            
            g_elf_ready = true;
            cout << "[ELF Thread] ELF is ready" << endl;
            
            // 等待ELF进程结束（线程会阻塞在这里）
            int status;
            waitpid(pid, &status, 0);
            
            g_elf_ready = false;
            
            if (WIFEXITED(status)) {
                cout << "[ELF Thread] ELF process exited with status: " 
                     << WEXITSTATUS(status) << endl;
            } else if (WIFSIGNALED(status)) {
                cout << "[ELF Thread] ELF process killed by signal: " 
                     << WTERMSIG(status) << endl;
            }
            
            // 如果主程序还在运行，尝试重启
            if (g_running) {
                cout << "[ELF Thread] ELF process died, restarting in 3 seconds..." << endl;
                sleep(3);
            }
        } 
        else {
            // fork失败
            cerr << "[ELF Thread] Fork failed" << endl;
            break;
        }
    }
    
    cout << "[ELF Thread] ELF daemon thread exiting" << endl;
}
int main(int argc, char* argv[]){

    if (argc < 2)
    {
        printf("Usage: %s <camera index> [target_ip] [port]\n", argv[0]);
        printf("Example: %s 0 192.168.1.100 8888\n", argv[0]);
        return -1;
    }


    
    // // 设置主程序为高优先级
    nice(-10);
    
    // 设置信号处理
    signal(SIGINT, signal_handler);   // Ctrl+C
    signal(SIGTERM, signal_handler);  // kill命令
    
    // 初始化UDP传输
    const char* target_ip = (argc >= 3) ? argv[2] : "192.168.100.3";
    int udp_port = (argc >= 4) ? atoi(argv[3]) : 8888;
    
    cout << "[Main] Initializing UDP transmission to " << target_ip << ":" << udp_port << endl;
    if (!init_udp_transmission(target_ip, udp_port)) {
        cerr << "[Main] UDP initialization failed, continuing without network transmission" << endl;
    } else {
        cout << "[Main] UDP transmission initialized successfully" << endl;
    }
    
    // 1. 启动ELF守护进程线程

    // cout << "[Main] Starting ELF daemon thread..." << endl;
    // std::thread elf_thread(elf_daemon_thread);

    // // 等待ELF初始化
    // for (int i = 0; i < 5; i++) {
    //     if (g_elf_ready) {
    //         cout << "[Main] ELF is ready" << endl;
    //         break;
    //     }
    //     sleep(1);
    // }
    
    // if (!g_elf_ready) {
    //     cerr << "[Main] ELF initialization timeout" << endl;
    //     g_running = false;
    //     if (elf_thread.joinable()) elf_thread.join();
    //     return -1;
    // }

    
    // 2. 打开 framebuffer
    cout << "[Main] Opening framebuffer..." << endl;
    int fbfd = open("/dev/fb0", O_RDWR);
    if (fbfd < 0) {
        printf("Cannot open /dev/fb0\n");
        g_running = false;
        // if (elf_thread.joinable()) elf_thread.join();
        return -1;
    }

    struct fb_fix_screeninfo finfo;
    struct fb_var_screeninfo vinfo;

    ioctl(fbfd, FBIOGET_FSCREENINFO, &finfo);
    ioctl(fbfd, FBIOGET_VSCREENINFO, &vinfo);

    // ★★★★★ 添加这行来打印物理地址 ★★★★★
    printf("Framebuffer Physical Address: 0x%lx\n", finfo.smem_start);
    printf("Framebuffer Size: %lu bytes (%.2f MB)\n", 
        finfo.smem_len, finfo.smem_len / (1024.0 * 1024.0));

    long screensize = vinfo.yres * finfo.line_length;
    printf("Framebuffer Virtual Size: %ld bytes (based on line length)\n", screensize);

    printf("Framebuffer: %dx%d  line_length=%d  bpp=%d\n",
        vinfo.xres, vinfo.yres, finfo.line_length, vinfo.bits_per_pixel);

    // 检查实际映射大小
    printf("Actual mapping size: %ld bytes\n", vinfo.yres_virtual * finfo.line_length);


    // long screensize = vinfo.yres * finfo.line_length;
    uint8_t* fbp = (uint8_t*)mmap(0, screensize,
        PROT_READ | PROT_WRITE,
        MAP_SHARED, fbfd, 0);

    if ((int)fbp == -1) {
        printf("Framebuffer mmap failed\n");
        close(fbfd);
        g_running = false;
        // if (elf_thread.joinable()) elf_thread.join();
        return -1;
    }

    printf("Framebuffer: %dx%d  line_length=%d  bpp=%d\n",
        vinfo.xres, vinfo.yres, finfo.line_length, vinfo.bits_per_pixel);

    // 3. 打开摄像头
    // cout << "[Main] Opening camera..." << endl;
    // VideoCapture cap;
    // if (isdigit(argv[1][0]))
    // {
    //     cap.open(argv[1][0] - '0');
    //     if (!cap.isOpened())
    //     {
    //         printf("Cannot open camera %s\n", argv[1]);
    //         g_running = false;
    //         // if (elf_thread.joinable()) elf_thread.join();
    //         munmap(fbp, screensize);
    //         close(fbfd);
    //         return -1;
    //     }
    // }

    // int cam_w = cap.get(CAP_PROP_FRAME_WIDTH);
    // int cam_h = cap.get(CAP_PROP_FRAME_HEIGHT);

    // printf("Camera resolution: %d x %d\n", cam_w, cam_h);
    int camera_index = atoi(argv[1]);
    // 启动摄像头线程
    global_capture_thread = std::thread(cameraCaptureThread, camera_index);
    
    // 等待摄像头初始化
    std::this_thread::sleep_for(std::chrono::milliseconds(500));


    unsigned char* pBuffer = (unsigned char*)malloc(DETECT_BUFFER_SIZE);

    Mat  small, bgra, output_frame,normalizing_rot;
    // 视线向量
    int theta, phi;
    float eye_center_x,eye_center_y,eye_center_z;
    // 人脸框
    int x,y,w,h;
    // 4. 映射共享内存
    cout << "[Main] Mapping shared memory..." << endl;
    int fd;
    if ((fd = open("/dev/mem", O_RDWR | O_SYNC)) == -1)
    {
        fprintf(stderr, "Error at line %d, file %s (%d) [%s]\n", __LINE__, __FILE__, errno, strerror(errno));
        g_running = false;
        // if (elf_thread.joinable()) elf_thread.join();
        munmap(fbp, screensize);
        close(fbfd);
        free(pBuffer);
        return -1;
    }
    
    size_t map_length = 36 * 60 * 2+(FRAME_BASE % MAP_SIZE);
    void* frame_base_eye = mmap(0, (map_length + MAP_SIZE - 1), PROT_READ | PROT_WRITE, MAP_SHARED, fd, FRAME_BASE & ~MAP_MASK);
    if(frame_base_eye == (void *) -1)
    {
        fprintf(stderr, "Error at line %d, file %s (%d) [%s]\n", __LINE__, __FILE__, errno, strerror(errno));
        close(fd);
        g_running = false;
        // if (elf_thread.joinable()) elf_thread.join();
        munmap(fbp, screensize);
        close(fbfd);
        free(pBuffer);
        return -1;
    }

    // void* frame_base_face = mmap(0, MAP_SIZE*3 * 64 * 64, PROT_READ | PROT_WRITE, MAP_SHARED, fd, (FRAME_BASE+0x01000000) & ~MAP_MASK);
    // if(frame_base_face == (void *) -1)
    // {
    //     fprintf(stderr, "Error at line %d, file %s (%d) [%s]\n", __LINE__, __FILE__, errno, strerror(errno));
    //     close(fd);
    //     g_running = false;
    //     // if (elf_thread.joinable()) elf_thread.join();
    //     munmap(fbp, screensize);
    //     close(fbfd);
    //     free(pBuffer);
    //     return -1;
    // }

    // 5. 主循环
    cout << "[Main] Starting main loop..." << endl;
    int frame_count = 0;
    

    // 用于网络传输的帧（可选调整大小以减少带宽）
    cv::Mat network_frame;
    bool resize_for_network = true;  // 是否调整大小以节省带宽
    int network_width = 640;         // 网络传输的宽度
    int network_height = 480;        // 网络传输的高 
    //read net
    std::string command = "./read.elf";
    //system(command.c_str());

    TickMeter readtm;
    readtm.start();
    if (action_elf(command) != 0) {
       std::cerr << "ELF 执行失败" << std::endl;
       return false;
    }
    // write_register(TRANSFER_ADDR, (get_register(TRANSFER_ADDR))|0x1);
    // while(((get_register(TRANSFER_ADDR)>>2)&0x1) != 0x1);
    readtm.stop();    
    printf("readweight_time = %gms\n", readtm.getTimeMilli());

    //--------------图片模式------------//如果需要视频把这段代码注释，开启下面的视频模式代码 
    // cv::Mat frame = cv::imread("wu2.jpg", cv::IMREAD_COLOR);
    // if (frame.empty()) {
    //     std::cerr << "Error: Could not open image file!" << std::endl;
    // }

    while (g_running)
    {
        frame_count++;
        //计时
        TickMeter pltm;
        pltm.start(); 
        // // 检查ELF是否还在运行
        // if (!g_elf_ready) {
        //     cerr << "[Main] ELF is not ready! Frame: " << frame_count << endl;
            
        //     // 等待一段时间，看ELF是否会恢复
        //     for (int i = 0; i < 3 && g_running; i++) {
        //         sleep(1);
        //         if (g_elf_ready) break;
        //     }
            
        //     if (!g_elf_ready) {
        //         cerr << "[Main] ELF not recovered, exiting..." << endl;
        //         g_running = false;
        //         break;
        //     }
        // }
        
        // 连续读取3次，只保留最后一次
        // cv::Mat frame1, frame2;
        
        // cap >> frame1;  // 丢弃
        // cap >> frame2;  // 丢弃
        // cap >> frame;   // 这是最新的

        //--------------视频模式------------//如果需要图片把这段代码注释，开启上面的图片模式代码        
        cv::Mat frame;

        bool got_frame = false;
        
        {
            std::lock_guard<std::mutex> lock(global_frame_mutex);
            if (global_frame_ready && !global_current_frame.empty()) {
                global_current_frame.copyTo(frame);
                global_frame_ready = false;  // 标记为已读取
                got_frame = true;
            }
        }
        
        if (!got_frame) {
            // 没有新帧，跳过这次循环
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }
 
        //--------------视频模式------------//

        if (frame.empty()) {
            cerr << "[Main] Failed to capture frame" << endl;
            continue;
        }
        
        if (frame_count % 30 == 0) {
            cout << "[Main] Processing frame " << frame_count << endl;
        }


        // 处理帧
        TickMeter pftm;
        pftm.start();
        bool success = processFrame(frame, output_frame,x,y,w,h,theta, phi, pBuffer, frame_base_eye,normalizing_rot,eye_center_x,eye_center_y,eye_center_z);
        pftm.stop();    
        printf("ProcessFrame Time = %gms\n", pftm.getTimeMilli());

        if (!success) {
            cerr << "[Main] Frame processing failed at frame " << frame_count << endl;
        } else if (frame_count % 30 == 0) {
            cout << "[Main] Gaze estimation - Pitch: " << theta << ", Yaw: " << phi << endl;
        }
        // 转成 BGRA（fb是 BGRA8888）
        // cvtColor(output_frame, bgra, cv::COLOR_BGR2BGRA);



        // 计算居中位置
        // int start_x = (vinfo.xres - cam_w) / 2;
        // int start_y = (vinfo.yres - cam_h) / 2;

        // cout << "[Main] DISPLAY START" << endl;
        // //将图像复制到framebuffer
        // for (int y = 0; y < cam_h; y++)
        // {
        //     int screen_y = start_y + y;
        //     if (screen_y >= vinfo.yres) break;
    
        //     uint8_t* screen_row = fbp + screen_y * finfo.line_length;
        //     uint8_t* bgra_row = bgra.ptr(y);
    
        //     memcpy(screen_row + start_x * 4, bgra_row, cam_w * 4);         
        // }
        // cout << "[Main] DISPLAY DONE" << endl;

        // 通过UDP发送图像
        if (udp_initialized) {
            if (resize_for_network && (output_frame.cols != network_width || output_frame.rows != network_height)) {
                // 调整大小以减少带宽
                cv::resize(output_frame, network_frame, cv::Size(network_width, network_height));
            } else {
                network_frame = output_frame;
            }
            bool send_success = send_turbojpeg_fixed(normalizing_rot,network_frame,x,y,w,h,theta,phi,eye_center_x,eye_center_y,eye_center_z);

            // bool send_success = send_face_gaze_data(network_frame,x,y,w,h,pitch, yaw,70);  // 质量70
            if (!send_success && frame_count % 10 == 0) {
                cerr << "[Main] Failed to send frame via UDP" << endl;
            }
            cout << "[Main] NET DISPLAY DONE" << endl;
        }
        pltm.stop();    
        printf("ALL Time = %gms\n", pltm.getTimeMilli());   
        // 控制帧率
        // usleep(33000);  // 大约30fps
    }

    // 6. 清理资源
    cout << "[Main] Cleaning up resources..." << endl;
    cv::destroyAllWindows();
    // 停止ELF进程
    // if (g_elf_pid > 0) {
    //     kill(g_elf_pid, SIGTERM);
    // }
    
    // // 等待ELF线程结束
    // if (elf_thread.joinable()) {
    //     elf_thread.join();
    // }

    cleanup_udp();
    // 释放资源
    free(pBuffer);
    
    if (frame_base_eye != MAP_FAILED) {
        munmap(frame_base_eye, map_length + MAP_SIZE - 1);
    }
    
    // if (frame_base_face != MAP_FAILED) {
    //     munmap(frame_base_face, MAP_SIZE*3 * 64 * 64);
    // }
    
    if (fd >= 0) {
        close(fd);
    }
    
    if (fbp != MAP_FAILED) {
        munmap(fbp, screensize);
    }
    
    if (fbfd >= 0) {
        close(fbfd);
    }
    
    cout << "[Main] Application exited" << endl;
    return 0;
}
