#include "GazeWorker.h"
#include <fstream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <vector>              
#include <tuple>
using namespace cv;
using namespace std;
// ===========================================================
// 主流程函数：后台线程处理 BGR 图像
// ===========================================================
bool processFrame(const cv::Mat &bgr_frame, 
                              cv::Mat &output_frame, 
                              int &face_x,
                              int &face_y,
                              int &face_w,
                              int &face_h,
                              int &theta, 
                              int &phi,unsigned char* pBuffer,void* input_buffer,cv::Mat &normalizing_rot,
                              float& eye_center_x,float& eye_center_y,float& eye_center_z) 
{
    
    if (bgr_frame.empty()) {
        std::cerr << "输入图像为空" << std::endl;
        return false;
    }

    cv::Mat final_image = bgr_frame.clone();
    cv::Mat small_image;
    resize(final_image, small_image, Size(), SCALE,SCALE);
    // -------------- 人脸检测 ----------------------
    int x, y, w, h, e_x, e_y, e_w, e_h;
    std::vector<cv::Point2f> image_points;

    std::tie(x, y, w, h, e_x, e_y, e_w, e_h, image_points)
        = libface_detection(small_image,pBuffer);

    if (x <= 0 || w <= 0 || h <= 0)
    {
        // 没有人脸，直接返回原始图像
        cv::Mat display = final_image.clone();
        cv::flip(display, display, 1);
        output_frame = display;
        return false;
    }

    //------cut eyes ROI-------
    Mat face_img,eye_roi;
    face_img = CutOut_Mat(final_image, x, y, w, h);
    resize(face_img, face_img, Size(64, 64));

    //----- input image is face ROI not a origin image!-----
    eye_roi = extractEyeROI(eye_center_x,eye_center_y,eye_center_z,final_image, image_points, model_points, camera_matrix, dist_coeffs, normalized_cam, Size(60, 36),normalizing_rot);
    cout << "Rotation matrix: " << normalizing_rot << endl;

    cv::cvtColor(eye_roi, eye_roi, cv::COLOR_BGR2GRAY);//BGR2GRAY
    cv::equalizeHist(eye_roi, eye_roi); 
    // while(((get_register(TRANSFER_ADDR)>>2)&0x1) != 0x1);
    // write_register(TRANSFER_ADDR, (get_register(TRANSFER_ADDR))&(~(0x1<<2)));

    cv::Mat normalized_float;
    eye_roi.convertTo(normalized_float, CV_32F, 1.0 / 255.0); // normalize

    Mat eye_roi_temp = convert_normalized_to_fixed(normalized_float, 15);

    // cout << "eye roi input :" << eye_roi_temp.at<int16_t>(35, 59) << endl;

    // // transfer to 16 dingdian
    int len_eye = 36 * 60 * 2;
    // // int fd;
    // // if ((fd = open("/dev/mem", O_RDWR | O_SYNC)) == -1)
    // // {
    // //     fprintf(stderr, "Error at line %d, file %s (%d) [%s]\n", __LINE__, __FILE__, errno, strerror(errno));
    // //     return;
    // // }
    // // size_t map_length = 36*60*2+(FRAME_BASE % MAP_SIZE);
    // // void* frame_base_eye = mmap(0, (map_length + MAP_SIZE - 1), PROT_READ | PROT_WRITE, MAP_SHARED, fd, FRAME_BASE & ~MAP_MASK);
    
    memcpy(input_buffer, eye_roi_temp.data, len_eye); //[60 * 36]

    // write_register(TRANSFER_ADDR, (get_register(TRANSFER_ADDR))|0x1);
    // std::string command = "./linux_ov5640.elf";
    std::string command = "./no_shortcut.elf";
    // std::string command = "./young.elf";
    //system(command.c_str());

    TickMeter pltm;
    pltm.start();
    if (action_elf(command) != 0) {
       std::cerr << "ELF 执行失败" << std::endl;
       return false;
    }
    // write_register(TRANSFER_ADDR, (get_register(TRANSFER_ADDR))|0x1);
    // while(((get_register(TRANSFER_ADDR)>>2)&0x1) != 0x1);
    pltm.stop();    
    printf("action_elf_time = %gms\n", pltm.getTimeMilli());

    // -------------- 裁剪人脸 ----------------------
    // cv::Mat face_img = CutOut_Mat(bgr_frame, x, y, w, h);
    // if (face_img.empty()) {
    //     std::cerr << "人脸裁剪失败" << std::endl;
    //     return false;
    // }

    //-------------- 预处理 ----------------------
    // cv::Mat processed_face = preprocess_face(face_img);
    // if (processed_face.empty()) {
    //     std::cerr << "人脸预处理失败" << std::endl;
    //     return false;
    // }

    //-------------- 保存 bin（模型输入）----------------------
    // if (!saveFaceToBin(processed_face)) {
    //     std::cerr << "bin 保存失败" << std::endl;
    //     return false;
    // }


    // -------------- 执行 ELF ----------------------
    // if (action_elf("./112V3_FULL_linux.elf") != 0) {
    //     std::cerr << "ELF 执行失败" << std::endl;
    //     return false;
    // }
    // TickMeter pltm;
    // pltm.start();
    // if (action_elf(command) != 0) {
    //    std::cerr << "ELF 执行失败" << std::endl;
    //    return false;
    // }
    // //write_register(TRANSFER_ADDR, (get_register(TRANSFER_ADDR))|0x1);
    // //while(((get_register(TRANSFER_ADDR)>>2)&0x1) != 0x1);
    // pltm.stop();    
    // printf("action_elf_time = %gms\n", pltm.getTimeMilli());

    // -------------- 从 DDR 读取 yaw / pitch ----------------------
//     pitch = 0.0f;
//     yaw   = 0.0f;
// //PL
//     Devmem_Read_Float(0x00101000, &pitch, 1);
//     Devmem_Read_Float(0x00101004, &yaw, 1);

    int16_t fc_int_buffer2[2];
    Devmem_Read_int16(0x24031000, fc_int_buffer2, 2,0);
    // pitch = (float)(fc_int_buffer2[0] + -3747)/32768;
    // yaw = (float)(fc_int_buffer2[1] + 1257)/32768;
    theta = fc_int_buffer2[0];
    phi   = fc_int_buffer2[1];
    // theta = (float)(fc_int_buffer2[0] + -3747)/32768;
    // phi = (float)(fc_int_buffer2[1] + 1257)/32768;
    // cv::Mat normalized_gaze_vector = (cv::Mat_<double>(1, 3) <<
    //             - (cos(theta) * sin(phi)),
    //             - sin(theta),
    //             - (cos(theta) * cos(phi))
    // );
    // cv::Mat gaze_vector = normalized_gaze_vector * normalizing_rot;
  
    // if (Devmem_Read_Float(0x00101000, &pitch, 1) != 0) {
    //     std::cerr << "读取pitch失败" << std::endl;
    // }
    
    // if (Devmem_Read_Float(0x00101004, &yaw, 1) != 0) {
    //     std::cerr << "读取yaw失败" << std::endl;
    // }

    // -------------- 画视线 ----------------------
    cv::Mat display_img = bgr_frame.clone();
    // cv::flip(display_img, display_img, 1);
    
    // 计算镜像后的坐标
    // int flipped_x = display_img.cols - x - w;
    // int flipped_y = y;
    // int flipped_w = w;
    // int flipped_h = h;
    face_x = x* CUT_THRESHOLD;
    face_y = y* CUT_THRESHOLD;
    face_w = w* CUT_THRESHOLD;
    face_h = h* CUT_THRESHOLD;
    
    // // 绘制人脸框
    // cv::rectangle(display_img,
    //               cv::Rect(flipped_x, flipped_y, flipped_w, flipped_h),
    //               cv::Scalar(0, 255, 0),
    //               2);
    
    // // 绘制视线
    // drawGaze(display_img, flipped_x, flipped_y, flipped_w, flipped_h, -pitch, yaw);
    
    // 添加姿态信息文本
    // std::string text = "Pitch: " + std::to_string(pitch) + 
    //                    "  Yaw: " + std::to_string(yaw);
    // cv::putText(display_img, text, cv::Point(10, 30), 
    //             cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(0, 255, 0), 2);

    output_frame = display_img;
    // output_frame = final_image.clone();
    printf("gazework done\n");
    return true;
}

// 人脸检测：输出人脸特征点信息
// ===========================================================
std::tuple<int, int, int, int, int, int, int, int, std::vector<cv::Point2f>> libface_detection(Mat input_img,unsigned char* pBuffer)
{
    Mat image = input_img.clone();
    int x = -2, y = -2, w = -2, h = -2;
    float e_x = -3, e_y = -3, e_w = -3, e_h = -3;
    int *pResults = NULL;
    vector<Point2f> image_points;

    TickMeter cvtm;
    cvtm.start();
    pResults = facedetect_cnn(pBuffer, (unsigned char*)(image.ptr(0)), image.cols, image.rows, (int)image.step);
    printf("[libface_detection]detected %d faces .\n", (pResults ? *pResults : 0));
    cvtm.stop();    
    printf("facedetect_cnn_time = %gms\n", cvtm.getTimeMilli());
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
// ===========================================================
// BGR → RGB → 448×448 → normalize
// ===========================================================
cv::Mat preprocess_face(const cv::Mat &face_bgr)
{
    if (face_bgr.empty()) return cv::Mat();

    cv::Mat face_rgb;
    cv::cvtColor(face_bgr, face_rgb, cv::COLOR_BGR2RGB);

    cv::Mat resized;
    cv::resize(face_rgb, resized, cv::Size(448, 448));

    cv::Mat img_float;
    resized.convertTo(img_float, CV_32FC3, 1.0 / 255.0);

    std::vector<cv::Mat> ch(3);
    cv::split(img_float, ch);

    for (int i = 0; i < 3; i++) {
        ch[i] = (ch[i] - NORM_MEAN[i]) / NORM_STD[i];
    }

    cv::Mat output;
    cv::merge(ch, output);
    return output;
}

// ===========================================================
// 裁剪人脸（安全版本）
// ===========================================================
cv::Mat CutOut_Mat(const cv::Mat &img, int x, int y, int w, int h)
{
    if (img.empty()) return cv::Mat();

    cv::Rect roi(x, y, w, h);
    cv::Rect full(0, 0, img.cols, img.rows);

    cv::Rect valid = roi & full;  // 求交集（最安全）

    if (valid.width <= 0 || valid.height <= 0)
        return cv::Mat();

    return img(valid).clone();
}


// ===========================================================
// 保存模型输入到 DDR bin 文件
// ===========================================================
bool saveFaceToBin(const cv::Mat &input)
{
    // 1. 基本检查（必须是预处理后的 448×448×3 float32）
    if (input.empty() ||
        input.rows != 448 ||
        input.cols != 448 ||
        input.type() != CV_32FC3)
    {
        printf("[BinSave] 输入图像不合法（必须是 448x448, CV_32FC3）\n");
        return false;
    }

    const int N = 1, C = 3, H = 448, W = 448;
    const size_t totalSize = N * C * H * W;

    // 2. 高效 NCHW 缓冲区（静态避免重复分配）
    static std::vector<float> nchw(totalSize);

    float *dst = nchw.data();

    // 3. HWC -> NCHW（高性能写法）
    // input(h,w,c) => dst[c*H*W + h*W + w]
    for (int h = 0; h < H; ++h)
    {
        const cv::Vec3f *rowPtr = input.ptr<cv::Vec3f>(h);

        for (int w = 0; w < W; ++w)
        {
            float r = rowPtr[w][0];
            float g = rowPtr[w][1];
            float b = rowPtr[w][2];

            // 写入 NCHW
            dst[(0 * H + h) * W + w] = r;  // C0
            dst[(1 * H + h) * W + w] = g;  // C1
            dst[(2 * H + h) * W + w] = b;  // C2
        }
    }

    // 4. 写入 bin（float32）
    FILE *fp = fopen("/mnt/model_input.bin", "wb");
    if (!fp)
    {
        printf("[BinSave] 打开文件失败！\n");
        return false;
    }

    fwrite(dst, sizeof(float), nchw.size(), fp);
    fclose(fp);

    printf("[BinSave] Success: /mnt/model_input.bin\n");
    return true;
}


// ===========================================================
// 执行 ELF（阻塞，但在后台线程，不影响 UI）
// ===========================================================
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


// ===========================================================
// 从 DDR 读取 float
// ===========================================================
int Devmem_Read_Float(unsigned long addr, float *out, int count)
{
    int fd = open("/dev/mem", O_RDONLY | O_SYNC);
    if (fd < 0) return -1;

    unsigned long page = addr & ~(4095);
    unsigned long offset = addr - page;

    void *map_base = mmap(NULL, 4096, PROT_READ, MAP_SHARED, fd, page);
    if (map_base == MAP_FAILED) {
        close(fd);
        return -1;
    }

    float *ptr = (float*)((char*)map_base + offset);
    for (int i = 0; i < count; i++) out[i] = ptr[i];

    munmap(map_base, 4096);
    close(fd);
    return 0;
}

// ===========================================================
// 在图像上画视线（简单线段版）
// ===========================================================
void drawGaze(cv::Mat &img, int x, int y, int w, int h,
                          float pitch, float yaw)
{
    cv::Point center(x + w/2, y + h/2);
    float len = w * 0.8;

    float dx =  len * sin(yaw);
    float dy = -len * sin(pitch);

    cv::Point end(center.x + (int)dx, center.y + (int)dy);
    cv::line(img, center, end, cv::Scalar(0,255,0), 2);
}



int16_t float_to_int16(float value, int n)
{
    if(value == 1){
        return 32767;
    }
    else{
        float scaled = value * (1 << n);
        int16_t fixed_point = round(scaled);
        return fixed_point;
    }
}

Mat convert_normalized_to_fixed(cv::Mat& src, int n) {
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

int Devmem_Read_int16(unsigned long readAddr, int16_t* buf, unsigned long len ,int status)//0_read  1_write
{
    int i = 0;
    int fd;
//    int offset_len = 0;
    void *map_base, *virt_addr;
    off_t addr = readAddr;
//	unsigned long littleEndianLength = 0;
    unsigned long page_addr = addr & ~MAP_MASK;
    if ((fd = open("/dev/mem", O_RDWR | O_SYNC)) == -1)
    {
        fprintf(stderr, "Error at line %d, file %s (%d) [%s]\n", __LINE__, __FILE__, errno, strerror(errno));
        return 0;
    }

    /* Map one page */ 
    map_base = mmap(0, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, addr & ~MAP_MASK);
    if(map_base == (void *) -1)
    {
        fprintf(stderr, "Error at line %d, file %s (%d) [%s]\n", __LINE__, __FILE__, errno, strerror(errno));
        close(fd);
        return 0;
    }

    for (i = 0; i < len; i++)
    {
        
//        if(offset_len >= MAP_MASK)
        if((addr & ~MAP_MASK) != page_addr)
        {
//            offset_len = 0;
            page_addr = addr & ~MAP_MASK;
            if(munmap(map_base, MAP_SIZE) == -1)
            {
                fprintf(stderr, "Error at line %d, file %s (%d) [%s]\n", __LINE__, __FILE__, errno, strerror(errno));
                close(fd);
                return 0;
            }
            map_base = mmap(0, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, addr & ~MAP_MASK);
            if(map_base == (void *) -1)
            {
                fprintf(stderr, "Error at line %d, file %s (%d) [%s]\n", __LINE__, __FILE__, errno, strerror(errno));
                close(fd);
                return 0;
            }
        }

        virt_addr = map_base + (addr & MAP_MASK);	
        if(status == 0){
            buf[i] = *((int16_t *) virt_addr);	
        }
        if(status == 1){
            *((int16_t *)virt_addr) = buf[i];
        }
        addr += 2;
//        offset_len += 4;
    }

    if(munmap(map_base, MAP_SIZE) == -1)
    {
        fprintf(stderr, "Error at line %d, file %s (%d) [%s]\n", __LINE__, __FILE__, errno, strerror(errno));
        close(fd);
        return 0;
    }
    close(fd);
    return i;
}
