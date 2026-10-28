#ifndef RW_H
#define RW_H

#include <stdint.h>
#define FACTOR 1024
typedef short data_t;

// 内存映射的大小，通常为4KB（内存页的标准大小）
// UL表示无符号长整型
// 用于计算页内偏移的掩码（4095 = 4096 - 1，二进制为0xFFF）
#define MAP_SIZE 4096UL
#define MAP_MASK (MAP_SIZE - 1)

// 定义MIN宏
#define MIN(a, b) ((a) < (b) ? (a) : (b))

// 从DDR读取数据
unsigned int Devmem_Read(unsigned long readAddr, data_t* buf, unsigned long len);

// 向DDR写入数据
unsigned int Devmem_Write(unsigned long writeAddr, data_t* buf, unsigned long len);

// 从DDR读取float数据
unsigned int Devmem_Read_Float(unsigned long readAddr, float* buf, unsigned long len);

// 向DDR写入float数据
unsigned int Devmem_Write_Float(unsigned long writeAddr, float* buf, unsigned long len);

#endif // RW_H
