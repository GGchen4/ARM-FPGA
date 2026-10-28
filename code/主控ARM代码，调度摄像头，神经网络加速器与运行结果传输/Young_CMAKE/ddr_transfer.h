#ifndef DDR_TRANSFER_H
#define DDR_TRANSFER_H

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <unistd.h>
#include <cstdlib>
#include <string>
using namespace std;
#define BRAM_CTRL_0 0x30001000
#define FRAME_BASE 0x10000000
#define DATA_LEN    10
#define MAP_SIZE 4096UL  			//映射的内存区大小（一般为一个叶框大小）
#define MAP_MASK (MAP_SIZE - 1)   	//MAP_MASK = 0XFFF

static int Devmem_Read(unsigned long readAddr, unsigned long* buf, unsigned long len ,int status);//0_read  1_write
int ddr_transfer(void *map_base0);

int action_elf(string command);

#endif // DDR_TRANSFER_H
