#!/usr/bin/env python3
"""
PC端接收程序 - 接收数据并绘制人脸框和视线
"""
from typing import Optional, Tuple
import socket
import cv2
import numpy as np
import struct
import time
import sys
from collections import deque
from scipy.spatial.transform import Rotation
from scipy.io import savemat


camera_matrix = np.array([640,0,320,0,640,240,0,0,1]).reshape(3, 3)
dist_coefficients = np.array([0., 0., 0., 0., 0.]).reshape(-1, 1)
length = 0.2

def _convert_pt(point: np.ndarray) -> Tuple[int, int]:
        return tuple(np.round(point).astype(int).tolist())

def project_points(points3d: np.ndarray,
                       rvec: Optional[np.ndarray] = None,
                       tvec: Optional[np.ndarray] = None) -> np.ndarray:
        assert points3d.shape[1] == 3
        if rvec is None:
            rvec = np.zeros(3, dtype=np.float64)
        if tvec is None:
            tvec = np.zeros(3, dtype=np.float64)
        points2d, _ = cv2.projectPoints(points3d, rvec, tvec,
                                        camera_matrix,
                                        dist_coefficients)
        return points2d.reshape(-1, 2)

def draw_gaze(image, face_x, face_y, face_w, face_h, point1: np.ndarray,point0: np.ndarray,color: Tuple[int, int, int] = (255, 255, 0), lw=1):
    """
    在图像上绘制视线方向
    
    参数:
        image: 输入图像
        face_x, face_y: 人脸框左上角坐标
        face_w, face_h: 人脸框宽高
        pitch: 俯仰角（弧度）
        yaw: 偏航角（弧度）
        length: 视线箭头长度
    """
    # 计算人脸中心点
    center_x = face_x + face_w // 2
    center_y = face_y + face_h // 2

    #第一种方法
    # 从2D像素坐标反投影到3D相机坐标
    # 反投影公式: (x, y, 1) -> (x - cx)/fx, (y - cy)/fy, 1
    # 将人脸中心点转换到3D相机坐标系
    # 假设人脸在距离相机一定距离的平面上，这里假设深度为1.0
    z_depth = 0  # 深度可以调整
    fx = camera_matrix[0, 0]
    fy = camera_matrix[1, 1]
    cx = camera_matrix[0, 2]
    cy = camera_matrix[1, 2]
    # point0 = np.array([
    #     (center_x - cx) / fx * z_depth,
    #     (center_y - cy) / fy * z_depth,
    #     point0[2]
    # ], dtype=np.float64)
    # 3D终点：起点沿着视线方向移动一定距离
    point1_3d = point0 + point1 * length
    # 将3D点投影到2D图像平面
    points3d = np.vstack([point0, point1_3d])
    points2d = project_points(points3d)

    # 绘制视线
    pt0 = _convert_pt(points2d[0])
    pt1 = _convert_pt(points2d[1])
    cv2.line(image, pt0, pt1, color, lw, cv2.LINE_AA)
    # 根据视线角度计算终点
    # 注意：这里可能需要根据你的坐标系统调整符号
    # end_x = int(center_x + length * np.sin(yaw))
    # end_y = int(center_y + length * np.sin(pitch))
    
    # # 绘制视线箭头
    # cv2.arrowedLine(image, (center_x, center_y), (end_x, end_y), 
    #                (0, 255, 0), 2, tipLength=0.3)
    
    # 绘制中心点
    cv2.circle(image, (center_x, center_y), 3, (0, 0, 255), -1)

    # #第二种方法
    # # 简化：将3D视线向量映射到2D图像
    # # 使用x和y分量决定方向，忽略深度影响
    # arrow_length = 100

    # dx = point1[0] * arrow_length
    # dy = point1[1] * arrow_length
    
    # # 计算终点
    # end_x = int(center_x + dx)
    # end_y = int(center_y + dy)
    
    # # 绘制视线
    # cv2.arrowedLine(image, 
    #                 (center_x, center_y),
    #                 (end_x, end_y),
    #                 (0, 255, 0), 2, tipLength=0.3)
    
    # # 绘制起点
    # cv2.circle(image, (center_x, center_y), 5, (0, 0, 255), -1)
    
    return image

def receive_and_draw_video(port=8888):
    """接收数据并在PC端绘制"""
    # 创建UDP socket
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 1024 * 1024)
    sock.bind(('0.0.0.0', port))
    sock.settimeout(3.0)
    
    print("=" * 60)
    print("PC端视线估计显示系统")
    print(f"监听端口: {port}")
    print("从开发板接收数据并绘制人脸框和视线")
    print("按 'q' 退出")
    print("=" * 60)
    
    # 创建显示窗口
    cv2.namedWindow('Face&Gaze', cv2.WINDOW_NORMAL)
    cv2.resizeWindow('Face&Gaze', 800, 600)
    
    # 性能统计
    frame_count = 0
    start_time = time.time()
    fps = 0
    fps_history = deque(maxlen=30)  # 存储最近30个FPS值
    
    # 数据结构大小
    DATA_HEADER_SIZE = 8 + 16 + 8 + 8 + 12  # 8字节头部 + 16字节人脸框 + 8字节视线 + 8字节旋转矩阵参数 + 12字节人眼3d位置
    
    try:
        while True:
            try:
                # 接收完整数据包
                data, addr = sock.recvfrom(65536)
                
                if len(data) < DATA_HEADER_SIZE:
                    print(f"数据包太小: {len(data)} 字节")
                    continue
                
                # 解析数据结构
                offset = 0
                
                # 解析头部
                frame_id = struct.unpack('I', data[offset:offset+4])[0]
                frame_id = socket.ntohl(frame_id)
                offset += 4
                
                data_size = struct.unpack('I', data[offset:offset+4])[0]
                data_size = socket.ntohl(data_size)
                offset += 4
                
                # 检查数据完整性
                if len(data) - DATA_HEADER_SIZE != data_size:
                    print(f"数据大小不匹配: 期望 {data_size}, 实际 {len(data)-DATA_HEADER_SIZE}")
                    continue
                
                # 解析人脸框
                face_x = struct.unpack('I', data[offset:offset+4])[0]
                face_x = socket.ntohl(face_x)
                offset += 4
                
                face_y = struct.unpack('I', data[offset:offset+4])[0]
                face_y = socket.ntohl(face_y)
                offset += 4
                
                face_w = struct.unpack('I', data[offset:offset+4])[0]
                face_w = socket.ntohl(face_w)
                offset += 4
                
                face_h = struct.unpack('I', data[offset:offset+4])[0]
                face_h = socket.ntohl(face_h)
                offset += 4
                
                # 解析视线数据（转换为浮点数）
                pitch_i = struct.unpack('!i', data[offset:offset+4])[0]
                # pitch_i = socket.ntohl(pitch_i)
                print("pitch_i:", pitch_i)
                offset += 4

                # pitch_i = 2557
                pitch = (float)(pitch_i + -3747)/32768
                
                yaw_i  = struct.unpack('!i', data[offset:offset+4])[0]
                # yaw_i = socket.ntohl(yaw_i)
                print("yaw_i:", yaw_i)
                offset += 4

                # yaw_i = -2302
                yaw = (float)(yaw_i + 1257)/32768

                # 计算视线向量
                gaze_vector = np.array([
                    -(np.cos(pitch) * np.sin(yaw)),  # x
                    -np.sin(pitch),                   # y
                    -(np.cos(pitch) * np.cos(yaw))    # z
                ])
                # 打印gaze_vector
                print("Gaze vector (x, y, z):", gaze_vector)
                # pitch_bytes = data[offset:offset+4]
                # yaw_bytes = data[offset+4:offset+8]
                
                # # 转换为浮点数
                # pitch = struct.unpack('f', pitch_bytes)[0]
                # yaw = struct.unpack('f', yaw_bytes)[0]
                # offset += 8
                # 旋转矩阵的参数有8个字节
  
                # 提取旋转矩阵数据
                # 是否有旋转矩阵
                has_rot  = struct.unpack('I', data[offset:offset+4])[0]
                has_rot  = socket.ntohl(has_rot)
                offset += 4
                # 旋转矩阵大小
                rot_size  = struct.unpack('I', data[offset:offset+4])[0]
                rot_size  = socket.ntohl(rot_size)
                offset += 4

                # # 提取人眼3d位置（转换为浮点数）
                eye_center_x_int = struct.unpack('!i', data[offset:offset+4])[0]
                print("eye_center_x_int:", eye_center_x_int)
                # eye_center_x_int = socket.ntohl(eye_center_x_int)
                offset += 4
                eye_center_x = (float)(eye_center_x_int)/32768

                eye_center_y_int = struct.unpack('!i', data[offset:offset+4])[0]
                # eye_center_y_int = socket.ntohl(eye_center_y_int)
                print("eye_center_x_int:", eye_center_y_int)
                offset += 4
                eye_center_y = (float)(eye_center_y_int)/32768

                eye_center_z_int = struct.unpack('!i', data[offset:offset+4])[0]
                # eye_center_z_int = socket.ntohl(eye_center_z_int)
                print("eye_center_x_int:", eye_center_z_int)
                offset += 4
                eye_center_z = (float)(eye_center_z_int)/32768


                eye_center = np.array([
                    eye_center_x,  # x
                    eye_center_y,  # y
                    eye_center_z   # z
                ])
                print("eye_center (x, y, z):", eye_center)

                # 提取旋转矩阵数据
                normalizing_rot = None
                if has_rot and rot_size > 0:
                    rot_data = data[offset:offset+36]
                    if len(rot_data) == 36:  # 9个float，每个4字节
                        float_values = []
                        for i in range(0, 36, 4):
                            # 读取网络字节序的uint32
                            net_int = struct.unpack('!i', rot_data[i:i+4])[0]
                            # 转换为主机字节序
                            # host_uint = socket.ntohl(net_uint)
                            # 转换为float
                            # float_val = struct.unpack('f', struct.pack('I', host_uint))[0]
                            float_val = (float)(net_int)/32768
                            float_values.append(float_val)
            
            # 转换为3x3 numpy矩阵
                    normalizing_rot = np.array(float_values, dtype=np.float32).reshape(3, 3)
                else:
                    print(f"警告: 旋转矩阵数据大小异常: {len(rot_data)} 字节")

                print("normalizing_rot:", normalizing_rot)
                #归一化视线视线向量
                # normalized_gaze_vector = gaze_vector @ normalizing_rot
                normalized_gaze_vector = gaze_vector

                offset += 36
                # 提取图像数据
                image_data = data[offset:]
                
                # 解码图像
                img_array = np.frombuffer(image_data, dtype=np.uint8)
                frame = cv2.imdecode(img_array, cv2.IMREAD_COLOR)
                
                if frame is not None:
                    frame_count += 1
                    current_time = time.time()
                    
                    # 计算FPS
                    elapsed = current_time - start_time
                    if elapsed >= 1.0:
                        fps = frame_count / elapsed
                        fps_history.append(fps)
                        avg_fps = sum(fps_history) / len(fps_history) if fps_history else fps
                        
                        frame_count = 0
                        start_time = current_time
                    
                    # 在PC端绘制
                    display_frame = frame.copy()
                    # 1. 绘制人脸框
                    cv2.rectangle(display_frame,
                                 (face_x, face_y),
                                 (face_x + face_w, face_y + face_h),
                                 (0, 255, 0), 2)  # 绿色框
                    draw_gaze(display_frame, face_x, face_y, face_w, face_h, normalized_gaze_vector,eye_center)


                    # #翻转绘制
                    # display_frame = cv2.flip(display_frame, 1)  # 1 表示水平翻转
                    # # 获取图像尺寸
                    # height, width = display_frame.shape[:2]

                    # # 计算翻转后的人脸框坐标
                    # flipped_face_x = width - face_x - face_w
                    # flipped_face_y = face_y  # y坐标不变
                    # face_x = display_frame.shape[0] - face_x - face_w
 
                    
                    # # 1. 绘制人脸框（使用翻转后的坐标）
                    # cv2.rectangle(display_frame,
                    #              (flipped_face_x, flipped_face_y),
                    #              (flipped_face_x + face_w, flipped_face_y + face_h),
                    #              (0, 255, 0), 2)  # 绿色框
                    
                    # # 2. 绘制视线
                    # # draw_gaze(display_frame, face_x, face_y, face_w, face_h, normalized_gaze_vector)
                    # # 2. 绘制视线（使用翻转后的人脸框坐标）
                    # draw_gaze(display_frame, flipped_face_x, flipped_face_y, face_w, face_h, normalized_gaze_vector)
                    
                    # 3. 添加信息文本
                    info_lines = [
                        f"Frame: {frame_id}",
                        f"FPS: {fps:.1f} (Avg: {avg_fps:.1f})",
                        f"Face: ({face_x}, {face_y}) {face_w}x{face_h}",
                        f"Gaze: Pitch={pitch:.2f}, Yaw={yaw:.2f}",
                        f"Data: {len(data)} bytes",
                        f"From: {addr[0]}"
                    ]
                    
                    y_pos = 30
                    for i, line in enumerate(info_lines):
                        # 添加文字背景
                        text_size = cv2.getTextSize(line, cv2.FONT_HERSHEY_SIMPLEX, 0.6, 2)[0]
                        cv2.rectangle(display_frame, 
                                     (5, y_pos + i*25 - 20), 
                                     (5 + text_size[0] + 10, y_pos + i*25 + 5), 
                                     (0, 0, 0), -1)
                        
                        # 添加文字
                        cv2.putText(display_frame, line, (10, y_pos + i*25),
                                   cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 255, 255), 2)
                    
                    # 显示图像
                    cv2.imshow('Face&Gaze', display_frame)
                    
                    # 每30帧打印一次信息
                    if frame_id % 30 == 0:
                        h, w = frame.shape[:2]
                        print(f"Frame {frame_id}: {w}x{h}, FPS: {fps:.1f}, "
                              f"Face: {face_w}x{face_h}, Gaze: ({pitch:.2f}, {yaw:.2f})")
                else:
                    print("解码图像失败")
                
                # 检查是否退出
                if cv2.waitKey(1) & 0xFF == ord('q'):
                    print("\n用户退出")
                    break
                    
            except socket.timeout:
                # 显示等待画面
                wait_frame = np.zeros((480, 640, 3), dtype=np.uint8)
                cv2.putText(wait_frame, "Waitting Data...", (100, 200), 
                           cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 255, 255), 2)
                cv2.putText(wait_frame, f"port: {port}", (100, 250), 
                           cv2.FONT_HERSHEY_SIMPLEX, 0.8, (0, 255, 255), 2)
                cv2.imshow('Face&Gaze', wait_frame)
                
                if cv2.waitKey(1) & 0xFF == ord('q'):
                    break
                    
            except Exception as e:
                print(f"接收数据时出错: {e}")
                import traceback
                traceback.print_exc()
                continue
                
    except KeyboardInterrupt:
        print("\n程序被用户中断")
    finally:
        sock.close()
        cv2.destroyAllWindows()
        print("\n" + "=" * 60)
        print("PC端显示程序已停止")
        print("=" * 60)

# 性能测试版本
def performance_test_receiver(port=8888, duration=10):
    """性能测试接收器"""
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(('0.0.0.0', port))
    sock.settimeout(5.0)
    
    print("性能测试模式 - 运行10秒")
    print("不显示图像，只统计性能")
    
    frame_count = 0
    data_size_total = 0
    start_time = time.time()
    
    DATA_HEADER_SIZE = 12 + 16 + 8
    
    try:
        while time.time() - start_time < duration:
            try:
                data, addr = sock.recvfrom(65536)
                
                if len(data) >= DATA_HEADER_SIZE:
                    frame_count += 1
                    data_size_total += len(data)
                    
                    # 解析帧ID
                    frame_id = struct.unpack('I', data[:4])[0]
                    frame_id = socket.ntohl(frame_id)
                    
                    if frame_count % 30 == 0:
                        elapsed = time.time() - start_time
                        fps = frame_count / elapsed
                        avg_data_size = data_size_total / frame_count
                        print(f"帧 {frame_id}: FPS={fps:.1f}, "
                              f"平均数据大小={avg_data_size:.0f} 字节")
                
            except socket.timeout:
                continue
            except Exception as e:
                continue
    
    except KeyboardInterrupt:
        pass
    finally:
        sock.close()
        
        elapsed = time.time() - start_time
        fps = frame_count / elapsed
        avg_data_size = data_size_total / frame_count if frame_count > 0 else 0
        
        print("\n" + "=" * 60)
        print("性能测试结果:")
        print(f"  运行时间: {elapsed:.1f} 秒")
        print(f"  总帧数: {frame_count}")
        print(f"  平均FPS: {fps:.1f}")
        print(f"  总数据量: {data_size_total/1024/1024:.2f} MB")
        print(f"  平均每帧数据大小: {avg_data_size:.0f} 字节")
        print("=" * 60)

if __name__ == "__main__":
    import argparse
    
    parser = argparse.ArgumentParser(description='PC端视线估计显示系统')
    parser.add_argument('--port', type=int, default=8888, help='UDP端口号')
    parser.add_argument('--mode', choices=['display', 'performance'], default='display',
                       help='模式: display=显示模式, performance=性能测试')
    
    args = parser.parse_args()
    
    if args.mode == 'display':
        receive_and_draw_video(args.port)
    elif args.mode == 'performance':
        performance_test_receiver(args.port, duration=10)