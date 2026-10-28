#!/usr/bin/env python3
"""
UDP视频流接收程序 - 简化版
专门用于接收开发板发送的头部+数据格式
"""

import socket
import cv2
import numpy as np
import struct
import time
import sys

def receive_video(port=8888):
    """
    接收开发板视频流
    协议：开发板先发送8字节头部，再发送图像数据
    """
    # 创建UDP socket
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(('0.0.0.0', port))
    sock.settimeout(3.0)  # 3秒超时
    
    print("=" * 60)
    print(f"UDP视频接收器 - 端口 {port}")
    print("协议: 8字节头部 + JPEG图像数据")
    print("等待开发板视频流...")
    print("按 'q' 退出窗口")
    print("=" * 60)
    
    # 创建显示窗口
    cv2.namedWindow('Video Stream', cv2.WINDOW_NORMAL)
    cv2.resizeWindow('Video Stream', 800, 600)
    
    frame_count = 0
    start_time = time.time()
    fps = 0
    last_frame_time = 0
    
    try:
        while True:
            try:
                # 步骤1: 接收8字节头部
                header_data, addr = sock.recvfrom(8)
                
                if len(header_data) != 8:
                    print(f"错误: 头部长度不是8字节: {len(header_data)}")
                    continue
                
                # 解析头部: frame_counter(4字节) + data_size(4字节)
                try:
                    frame_counter, data_size = struct.unpack('II', header_data)
                    # 转换为本地字节序
                    frame_counter = socket.ntohl(frame_counter)
                    data_size = socket.ntohl(data_size)
                    
                    if data_size <= 0 or data_size > 100000:  # 合理的图像大小范围
                        print(f"错误: 无效的数据大小: {data_size}")
                        continue
                        
                except Exception as e:
                    print(f"解析头部失败: {e}")
                    continue
                
                # 步骤2: 接收图像数据
                # 注意: 这里可能会分多次接收，但UDP通常是一次性接收整个包
                image_data, addr = sock.recvfrom(data_size + 100)  # 稍微多接收一些
                
                if len(image_data) < data_size:
                    print(f"警告: 数据不完整: 期望 {data_size}, 收到 {len(image_data)}")
                    # 尝试继续接收剩余数据
                    remaining = data_size - len(image_data)
                    for _ in range(3):  # 最多尝试3次
                        try:
                            more_data, addr = sock.recvfrom(remaining)
                            image_data += more_data
                            remaining -= len(more_data)
                            if remaining <= 0:
                                break
                        except socket.timeout:
                            break
                
                # 步骤3: 解码和显示图像
                if len(image_data) >= data_size:
                    try:
                        # 解码JPEG
                        img_array = np.frombuffer(image_data, dtype=np.uint8)
                        frame = cv2.imdecode(img_array, cv2.IMREAD_COLOR)
                        
                        if frame is not None:
                            frame_count += 1
                            current_time = time.time()
                            last_frame_time = current_time
                            
                            # 计算FPS
                            elapsed = current_time - start_time
                            if elapsed >= 1.0:
                                fps = frame_count / elapsed
                                frame_count = 0
                                start_time = current_time
                            
                            # 显示信息
                            display_frame = frame.copy()
                            h, w = frame.shape[:2]
                            
                            # 在图像上添加信息
                            info_lines = [
                                f"Frame: {frame_counter}",
                                f"FPS: {fps:.1f}",
                                f"Size: {w}x{h}",
                                f"Data: {data_size} 字节",
                                f"Addr: {addr[0]}"
                            ]
                            
                            y_pos = 30
                            for i, line in enumerate(info_lines):
                                cv2.putText(display_frame, line, (10, y_pos + i*25),
                                           cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 255, 0), 2)
                            
                            cv2.imshow('Video Stream', display_frame)
                            
                            # 每30帧打印一次信息
                            if frame_counter % 30 == 0:
                                print(f"帧 {frame_counter}: {w}x{h}, FPS: {fps:.1f}, 大小: {data_size} 字节")
                            
                        else:
                            print("解码图像失败")
                            
                    except Exception as e:
                        print(f"处理图像时出错: {e}")
                else:
                    print(f"数据接收不完整: 期望 {data_size}, 收到 {len(image_data)}")
                
                # 检查是否要退出
                if cv2.waitKey(1) & 0xFF == ord('q'):
                    print("\n用户退出")
                    break
                    
            except socket.timeout:
                # 显示等待画面
                wait_frame = np.zeros((480, 640, 3), dtype=np.uint8)
                cv2.putText(wait_frame, "Watting Connect...", (100, 200), 
                           cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 255, 255), 2)
                cv2.putText(wait_frame, f"Port: {port}", (100, 250), 
                           cv2.FONT_HERSHEY_SIMPLEX, 0.8, (0, 255, 255), 2)
                cv2.putText(wait_frame, "Checking", (100, 300), 
                           cv2.FONT_HERSHEY_SIMPLEX, 0.8, (0, 255, 255), 2)
                cv2.imshow('Video Stream', wait_frame)
                
                # 检查是否要退出
                if cv2.waitKey(1) & 0xFF == ord('q'):
                    print("\n用户退出")
                    break
                    
            except Exception as e:
                print(f"接收数据时出错: {e}")
                continue
                
    except KeyboardInterrupt:
        print("\n程序被用户中断")
    finally:
        # 清理资源
        sock.close()
        cv2.destroyAllWindows()
        print("\n" + "=" * 60)
        print("视频接收器已停止")
        print("=" * 60)


# 更简单的版本，如果上面的不工作
def simple_receiver(port=8888):
    """
    最简单的接收器
    假设发送方直接将JPEG数据发送过来（没有头部）
    """
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(('0.0.0.0', port))
    sock.settimeout(2.0)
    
    print(f"简单接收器 - 端口 {port}")
    print("假设直接接收JPEG数据")
    
    cv2.namedWindow('视频流', cv2.WINDOW_NORMAL)
    
    try:
        while True:
            try:
                # 直接接收数据
                data, addr = sock.recvfrom(65536)
                
                # 尝试解码
                img_array = np.frombuffer(data, dtype=np.uint8)
                frame = cv2.imdecode(img_array, cv2.IMREAD_COLOR)
                
                if frame is not None:
                    cv2.imshow('视频流', frame)
                    print(f"收到 {len(data)} 字节，解码成功")
                else:
                    print(f"收到 {len(data)} 字节，但无法解码")
                
                if cv2.waitKey(1) & 0xFF == ord('q'):
                    break
                    
            except socket.timeout:
                print("等待数据...")
                if cv2.waitKey(1) & 0xFF == ord('q'):
                    break
            except Exception as e:
                print(f"错误: {e}")
                
    except KeyboardInterrupt:
        print("\n用户中断")
    finally:
        sock.close()
        cv2.destroyAllWindows()


# 调试版本：打印所有接收到的数据
def debug_receiver(port=8888):
    """调试用，打印所有接收到的数据信息"""
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(('0.0.0.0', port))
    sock.settimeout(1.0)
    
    print(f"调试接收器 - 端口 {port}")
    print("显示所有接收到的数据信息")
    print("按 Ctrl+C 退出")
    
    packet_count = 0
    start_time = time.time()
    
    try:
        while True:
            try:
                data, addr = sock.recvfrom(65536)
                packet_count += 1
                
                # 显示信息
                current_time = time.time()
                elapsed = current_time - start_time
                
                print(f"[{packet_count}] 时间: {current_time:.3f}, 来源: {addr}")
                print(f"     数据长度: {len(data)} 字节")
                
                # 尝试判断是否是头部
                if len(data) == 8:
                    print("     可能是头部 (8字节)")
                    try:
                        a, b = struct.unpack('II', data)
                        a_local = socket.ntohl(a)
                        b_local = socket.ntohl(b)
                        print(f"     解析: {a_local}, {b_local}")
                    except:
                        print("     无法解析为头部")
                
                # 尝试解码为图像
                elif len(data) > 100:  # 假设图像数据较大
                    print("     可能是图像数据")
                
                print()
                
            except socket.timeout:
                if packet_count > 0:
                    fps = packet_count / elapsed
                    print(f"超时，已接收 {packet_count} 个包，平均 {fps:.1f} 包/秒")
                packet_count = 0
                start_time = time.time()
                
    except KeyboardInterrupt:
        print("\n调试结束")
    finally:
        sock.close()


if __name__ == "__main__":
    import argparse
    
    parser = argparse.ArgumentParser(description='UDP video')
    parser.add_argument('--port', type=int, default=8888, help='UDP端口号')
    parser.add_argument('--mode', choices=['normal', 'simple', 'debug'], default='normal',
                       help='模式: normal=正常接收, simple=简单接收, debug=调试模式')
    
    args = parser.parse_args()
    
    if args.mode == 'normal':
        receive_video(args.port)
    elif args.mode == 'simple':
        simple_receiver(args.port)
    elif args.mode == 'debug':
        debug_receiver(args.port)