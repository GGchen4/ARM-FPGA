#!/usr/bin/env python3
"""
PC端接收程序 - 支持JPEG和YUV两种格式
"""

import socket
import cv2
import numpy as np
import struct
import time
import sys
from collections import deque

def draw_gaze(image, face_x, face_y, face_w, face_h, pitch, yaw, length=50.0):
    """绘制视线"""
    center_x = face_x + face_w // 2
    center_y = face_y + face_h // 2
    
    end_x = int(center_x + length * np.sin(yaw))
    end_y = int(center_y + length * np.sin(pitch))
    
    cv2.arrowedLine(image, (center_x, center_y), (end_x, end_y), 
                   (0, 255, 0), 2, tipLength=0.3)
    cv2.circle(image, (center_x, center_y), 3, (0, 0, 255), -1)
    
    return image

def receive_mixed_video(port=8888, expected_format="auto"):
    """
    接收混合格式视频
    expected_format: "jpeg", "yuv", 或 "auto"（自动检测）
    """
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 1024 * 1024)
    sock.bind(('0.0.0.0', port))
    sock.settimeout(3.0)
    
    print("=" * 60)
    print("PC端混合格式接收器")
    print(f"监听端口: {port}")
    print(f"期望格式: {expected_format} (auto=自动检测)")
    print("支持JPEG和YUV420格式")
    print("=" * 60)
    
    cv2.namedWindow('Video Stream', cv2.WINDOW_NORMAL)
    
    frame_count = 0
    start_time = time.time()
    fps = 0
    fps_history = deque(maxlen=30)
    
    # 数据结构大小
    DATA_HEADER_SIZE = 32  # FaceGazeData结构体大小
    
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
                if len(data) - DATA_HEADER_SIZE < data_size:
                    print(f"数据不完整: 期望 {data_size}, 实际 {len(data)-DATA_HEADER_SIZE}")
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
                
                # 解析视线数据
                pitch = struct.unpack('f', data[offset:offset+4])[0]
                offset += 4
                
                yaw = struct.unpack('f', data[offset:offset+4])[0]
                offset += 4
                
                # 提取图像数据
                image_data = data[offset:offset+data_size]
                
                # 解码图像
                frame = None
                format_used = "unknown"
                
                if expected_format == "jpeg" or expected_format == "auto":
                    # 尝试解码为JPEG
                    try:
                        img_array = np.frombuffer(image_data, dtype=np.uint8)
                        frame = cv2.imdecode(img_array, cv2.IMREAD_COLOR)
                        if frame is not None:
                            format_used = "jpeg"
                    except:
                        pass
                
                if frame is None and (expected_format == "yuv" or expected_format == "auto"):
                    # 尝试解码为YUV420
                    try:
                        # 计算可能的图像尺寸
                        # YUV420大小 = width * height * 1.5
                        # 我们需要猜测宽度和高度
                        
                        # 方法1: 尝试常见的分辨率
                        common_resolutions = [
                            (640, 480),   # 480p
                            (320, 240),   # 240p
                            (800, 600),   # SVGA
                            (1024, 768),  # XGA
                        ]
                        
                        for width, height in common_resolutions:
                            expected_yuv_size = int(width * height * 1.5)
                            if data_size == expected_yuv_size:
                                # 解码YUV420
                                yuv_array = np.frombuffer(image_data, dtype=np.uint8)
                                yuv_height = int(height * 1.5)
                                yuv_frame = yuv_array.reshape((yuv_height, width))
                                frame = cv2.cvtColor(yuv_frame, cv2.COLOR_YUV2BGR_I420)
                                format_used = f"yuv_{width}x{height}"
                                break
                        
                        # 方法2: 如果不知道分辨率，尝试从数据大小推断
                        if frame is None:
                            # 尝试计算可能的宽高
                            # data_size = width * height * 1.5
                            # 假设宽高比4:3
                            height = int(np.sqrt(data_size / 1.5 / (4/3)))
                            width = int(height * 4 / 3)
                            
                            if width > 0 and height > 0:
                                try:
                                    yuv_array = np.frombuffer(image_data, dtype=np.uint8)
                                    yuv_height = int(height * 1.5)
                                    yuv_frame = yuv_array.reshape((yuv_height, width))
                                    frame = cv2.cvtColor(yuv_frame, cv2.COLOR_YUV2BGR_I420)
                                    format_used = f"yuv_auto_{width}x{height}"
                                except:
                                    pass
                    
                    except Exception as e:
                        print(f"YUV解码失败: {e}")
                
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
                                 (0, 255, 0), 2)
                    
                    # 2. 绘制视线
                    draw_gaze(display_frame, face_x, face_y, face_w, face_h, -pitch, yaw)
                    
                    # 3. 添加信息文本
                    info_lines = [
                        f"Frame: {frame_id}",
                        f"FPS: {fps:.1f} (Avg: {avg_fps:.1f})",
                        f"Format: {format_used}",
                        f"Size: {frame.shape[1]}x{frame.shape[0]}",
                        f"Face: ({face_x}, {face_y}) {face_w}x{face_h}",
                        f"Gaze: Pitch={pitch:.2f}, Yaw={yaw:.2f}",
                        f"Data: {len(data)} bytes"
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
                    cv2.imshow('Video Stream', display_frame)
                    
                    # 每30帧打印一次信息
                    if frame_id % 30 == 0:
                        h, w = frame.shape[:2]
                        print(f"Frame {frame_id}: {w}x{h}, FPS: {fps:.1f}, "
                              f"Format: {format_used}")
                else:
                    print(f"解码图像失败，数据大小: {data_size} 字节")
                    # 显示原始数据信息
                    print(f"  前20字节: {image_data[:20].hex()}")
                
                # 检查是否退出
                if cv2.waitKey(1) & 0xFF == ord('q'):
                    print("\n用户退出")
                    break
                    
            except socket.timeout:
                # 显示等待画面
                wait_frame = np.zeros((480, 640, 3), dtype=np.uint8)
                cv2.putText(wait_frame, "Waiting Data...", (100, 200), 
                           cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 255, 255), 2)
                cv2.putText(wait_frame, f"Port: {port}", (100, 250), 
                           cv2.FONT_HERSHEY_SIMPLEX, 0.8, (0, 255, 255), 2)
                cv2.imshow('Video Stream', wait_frame)
                
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

# 调试工具：查看接收到的原始数据
def debug_data_packet(port=8888):
    """调试数据包"""
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(('0.0.0.0', port))
    sock.settimeout(2.0)
    
    print(f"调试数据包 - 端口 {port}")
    print("显示接收到的原始数据信息")
    
    packet_count = 0
    
    try:
        while True:
            try:
                data, addr = sock.recvfrom(65536)
                packet_count += 1
                
                print(f"\n[{packet_count}] 收到 {len(data)} 字节 来自 {addr}")
                
                if len(data) >= 8:
                    # 解析帧ID和数据大小
                    try:
                        frame_id = struct.unpack('I', data[:4])[0]
                        frame_id = socket.ntohl(frame_id)
                        
                        data_size = struct.unpack('I', data[4:8])[0]
                        data_size = socket.ntohl(data_size)
                        
                        print(f"  帧ID: {frame_id}")
                        print(f"  数据大小: {data_size} 字节")
                        print(f"  实际剩余数据: {len(data)-32} 字节" if len(data) >= 32 else f"  实际剩余数据: {len(data)-8} 字节")
                        
                        # 检查是否是常见的JPEG或YUV大小
                        if data_size < 100000:  # 小于100KB，可能是JPEG
                            print(f"  可能是JPEG格式 (大小: {data_size} 字节)")
                        else:
                            print(f"  可能是YUV格式 (大小: {data_size} 字节)")
                        
                        # 显示前32字节
                        hex_data = ' '.join(f'{b:02x}' for b in data[:min(32, len(data))])
                        print(f"  前32字节: {hex_data}")
                        
                    except Exception as e:
                        print(f"  解析失败: {e}")
                
            except socket.timeout:
                print(".", end="", flush=True)
                continue
                
    except KeyboardInterrupt:
        print("\n调试结束")
    finally:
        sock.close()

if __name__ == "__main__":
    import argparse
    
    parser = argparse.ArgumentParser(description='PC端混合格式接收器')
    parser.add_argument('--port', type=int, default=8888, help='UDP端口号')
    parser.add_argument('--format', choices=['auto', 'jpeg', 'yuv'], default='auto',
                       help='期望格式: auto=自动检测, jpeg=JPEG格式, yuv=YUV格式')
    parser.add_argument('--debug', action='store_true', help='调试模式')
    
    args = parser.parse_args()
    
    if args.debug:
        debug_data_packet(args.port)
    else:
        receive_mixed_video(args.port, args.format)