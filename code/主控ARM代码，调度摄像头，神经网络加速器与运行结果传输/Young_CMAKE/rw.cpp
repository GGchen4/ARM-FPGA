#include "rw.h"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
//#include <cerrno> // 对于 C++ 程序
//#include <cstring> // 对于 C++ 程序
#include"math.h"
/*
 * mmap函数说明：
 * void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset)
 * 参数含义：
 * - addr: 映射的起始虚拟地址（传0表示由系统自动分配）
 * - length: 映射的字节数
 * - prot: 内存保护方式（PROT_READ-可读，PROT_WRITE-可写等）
 * - flags: 映射标志（MAP_SHARED-修改会同步到文件/物理内存）
 * - fd: 要映射的文件描述符
 * - offset: 映射的文件偏移量（必须是4KB的整数倍，即页对齐）
 */

#include <errno.h>
#include <string.h>
#include <stdint.h>
/**
 * 从指定地址读取数据
 * @param readAddr 读取起始地址
 * @param buf 存储读取数据的缓冲区
 * @param len 要读取的数据长度(单位：data_t)
 * @return 成功返回len，失败返回0
 */
unsigned int Devmem_Read(unsigned long readAddr, data_t* buf, unsigned long len)
{
    int i = 0;
    int fd;
    void *map_base, *virt_addr;
    off_t addr = readAddr;
    unsigned long page_addr = addr & ~MAP_MASK;

    // 打开/dev/mem设备
    if ((fd = open("/dev/mem", O_RDWR | O_SYNC)) == -1)
    {
        fprintf(stderr, "Error at line %d, file %s (%d) [%s]\n", __LINE__, __FILE__, errno, strerror(errno));
        return 0;
    }
    // 映射初始内存页
    map_base = mmap(0, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, addr & ~MAP_MASK);
    if (map_base == (void *) -1)
    {
        fprintf(stderr, "Error at line %d, file %s (%d) [%s]\n", __LINE__, __FILE__, errno, strerror(errno));
        close(fd);
        return 0;
    }
    // 循环读取数据
    for (i = 0; i < len; i++)
    {
        // 检查是否需要跨页映射
        if ((addr & ~MAP_MASK) != page_addr)
        {
            page_addr = addr & ~MAP_MASK;
            // 解除之前的映射
            if (munmap(map_base, MAP_SIZE) == -1)
            {
                fprintf(stderr, "Error at line %d, file %s (%d) [%s]\n", __LINE__, __FILE__, errno, strerror(errno));
                close(fd);
                return 0;
            }
            // 映射新的内存页
            map_base = mmap(0, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, addr & ~MAP_MASK);
            if (map_base == (void *) -1)
            {
                fprintf(stderr, "Error at line %d, file %s (%d) [%s]\n", __LINE__, __FILE__, errno, strerror(errno));
                close(fd);
                return 0;
            }
        }
        // 计算虚拟地址并读取数据
        virt_addr = map_base + (addr & MAP_MASK);
        buf[i] = *((volatile data_t *)virt_addr);
        addr += 2;  // 移动到下一个int16_t地址
    }
    // 解除映射
    if (munmap(map_base, MAP_SIZE) == -1)
    {
        fprintf(stderr, "Error at line %d, file %s (%d) [%s]\n", __LINE__, __FILE__, errno, strerror(errno));
        close(fd);
        return 0;
    }
    close(fd);
    return len;  // 成功返回读取长度
}

/**
 * 向指定地址写入数据
 * @param writeAddr 写入起始地址
 * @param buf 包含要写入数据的缓冲区
 * @param len 要写入的数据长度(单位：data_t)
 * @return 成功返回len，失败返回0
 */
unsigned int Devmem_Write(unsigned long writeAddr, data_t* buf, unsigned long len)
{
    int i = 0;
    int fd;
    void *map_base, *virt_addr;
    off_t addr = writeAddr;
    unsigned long page_addr = addr & ~MAP_MASK;
    // 打开/dev/mem设备
    if ((fd = open("/dev/mem", O_RDWR | O_SYNC)) == -1)
    {
        fprintf(stderr, "Error at line %d, file %s (%d) [%s]\n", __LINE__, __FILE__, errno, strerror(errno));
        return 0;
    }
    // 映射初始内存页
    map_base = mmap(0, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, addr & ~MAP_MASK);
    if (map_base == (void *) -1)
    {
        fprintf(stderr, "Error at line %d, file %s (%d) [%s]\n", __LINE__, __FILE__, errno, strerror(errno));
        close(fd);
        return 0;
    }
    // 循环写入数据
    for (i = 0; i < len; i++)
    {
        // 检查是否需要跨页映射
        if ((addr & ~MAP_MASK) != page_addr)
        {
            page_addr = addr & ~MAP_MASK;
            // 解除之前的映射
            if (munmap(map_base, MAP_SIZE) == -1)
            {
                fprintf(stderr, "Error at line %d, file %s (%d) [%s]\n", __LINE__, __FILE__, errno, strerror(errno));
                close(fd);
                return 0;
            }
            // 映射新的内存页
            map_base = mmap(0, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, addr & ~MAP_MASK);
            if (map_base == (void *) -1)
            {
                fprintf(stderr, "Error at line %d, file %s (%d) [%s]\n", __LINE__, __FILE__, errno, strerror(errno));
                close(fd);
                return 0;
            }
        }
        // 计算虚拟地址并写入数据
        virt_addr = map_base + (addr & MAP_MASK);
        *((volatile data_t *)virt_addr) = buf[i];
        addr += 2;  // 移动到下一个int16_t地址
    }
    // 解除映射
    if (munmap(map_base, MAP_SIZE) == -1)
    {
        fprintf(stderr, "Error at line %d, file %s (%d) [%s]\n", __LINE__, __FILE__, errno, strerror(errno));
        close(fd);
        return 0;
    }
    close(fd);
    return len;  // 成功返回写入长度
}

/**
 * 向指定地址写入float数据
 * @param writeAddr 写入起始地址 (必须4字节对齐)
 * @param buf 包含要写入的float数据的缓冲区
 * @param len 要写入的float数据个数
 * @return 成功返回len，失败返回0
 */
unsigned int Devmem_Write_Float(unsigned long writeAddr, float* buf, unsigned long len)
{
    int i = 0;
    int fd;
    void *map_base, *virt_addr;
    off_t addr = writeAddr;
    unsigned long page_addr = addr & ~MAP_MASK;

    // 检查地址对齐 (float需要4字节对齐)
    if (addr & 0x3) {
        fprintf(stderr, "Error: Address 0x%lx is not 4-byte aligned for float writing\n", addr);
        return 0;
    }

    // 打开/dev/mem设备
    if ((fd = open("/dev/mem", O_RDWR | O_SYNC)) == -1)
    {
        fprintf(stderr, "Error at line %d, file %s (%d) [%s]\n", __LINE__, __FILE__, errno, strerror(errno));
        return 0;
    }

    // 映射初始内存页
    map_base = mmap(0, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, addr & ~MAP_MASK);
    if (map_base == (void *) -1)
    {
        fprintf(stderr, "Error at line %d, file %s (%d) [%s]\n", __LINE__, __FILE__, errno, strerror(errno));
        close(fd);
        return 0;
    }

    // 循环写入数据
    for (i = 0; i < len; i++)
    {
        // 检查是否需要跨页映射
        if ((addr & ~MAP_MASK) != page_addr)
        {
            page_addr = addr & ~MAP_MASK;
            // 解除之前的映射
            if (munmap(map_base, MAP_SIZE) == -1)
            {
                fprintf(stderr, "Error at line %d, file %s (%d) [%s]\n", __LINE__, __FILE__, errno, strerror(errno));
                close(fd);
                return 0;
            }
            // 映射新的内存页
            map_base = mmap(0, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, addr & ~MAP_MASK);
            if (map_base == (void *) -1)
            {
                fprintf(stderr, "Error at line %d, file %s (%d) [%s]\n", __LINE__, __FILE__, errno, strerror(errno));
                close(fd);
                return 0;
            }
        }

        // 计算虚拟地址并写入float数据
        virt_addr = map_base + (addr & MAP_MASK);
        *((volatile float *)virt_addr) = buf[i];
        addr += sizeof(float);  // 移动到下一个float地址
    }

    // 解除映射
    if (munmap(map_base, MAP_SIZE) == -1)
    {
        fprintf(stderr, "Error at line %d, file %s (%d) [%s]\n", __LINE__, __FILE__, errno, strerror(errno));
        close(fd);
        return 0;
    }

    close(fd);
    return len;  // 成功返回写入长度
}

/**
 * 从指定地址读取float数据
 * @param readAddr 读取起始地址 (必须4字节对齐)
 * @param buf 存储读取的float数据的缓冲区
 * @param len 要读取的float数据个数
 * @return 成功返回len，失败返回0
 */
unsigned int Devmem_Read_Float(unsigned long readAddr, float* buf, unsigned long len)
{
    int i = 0;
    int fd;
    void *map_base, *virt_addr;
    off_t addr = readAddr;
    unsigned long page_addr = addr & ~MAP_MASK;

    // 检查地址对齐 (float需要4字节对齐)
    if (addr & 0x3) {
        fprintf(stderr, "Error: Address 0x%lx is not 4-byte aligned for float reading\n", addr);
        return 0;
    }

    // 打开/dev/mem设备
    if ((fd = open("/dev/mem", O_RDWR | O_SYNC)) == -1)
    {
        fprintf(stderr, "Error at line %d, file %s (%d) [%s]\n", __LINE__, __FILE__, errno, strerror(errno));
        return 0;
    }

    // 映射初始内存页
    map_base = mmap(0, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, addr & ~MAP_MASK);
    if (map_base == (void *) -1)
    {
        fprintf(stderr, "Error at line %d, file %s (%d) [%s]\n", __LINE__, __FILE__, errno, strerror(errno));
        close(fd);
        return 0;
    }

    // 循环读取数据
    for (i = 0; i < len; i++)
    {
        // 检查是否需要跨页映射
        if ((addr & ~MAP_MASK) != page_addr)
        {
            page_addr = addr & ~MAP_MASK;
            // 解除之前的映射
            if (munmap(map_base, MAP_SIZE) == -1)
            {
                fprintf(stderr, "Error at line %d, file %s (%d) [%s]\n", __LINE__, __FILE__, errno, strerror(errno));
                close(fd);
                return 0;
            }
            // 映射新的内存页
            map_base = mmap(0, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, addr & ~MAP_MASK);
            if (map_base == (void *) -1)
            {
                fprintf(stderr, "Error at line %d, file %s (%d) [%s]\n", __LINE__, __FILE__, errno, strerror(errno));
                close(fd);
                return 0;
            }
        }

        // 计算虚拟地址并读取float数据
        virt_addr = map_base + (addr & MAP_MASK);
        buf[i] = *((volatile float *)virt_addr);
        addr += sizeof(float);  // 移动到下一个float地址
    }

    // 解除映射
    if (munmap(map_base, MAP_SIZE) == -1)
    {
        fprintf(stderr, "Error at line %d, file %s (%d) [%s]\n", __LINE__, __FILE__, errno, strerror(errno));
        close(fd);
        return 0;
    }

    close(fd);
    return len;  // 成功返回读取长度
}
