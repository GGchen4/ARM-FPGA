#include "ddr_transfer.h"
#include "catch_image.h"

static int Devmem_Read(unsigned long readAddr, unsigned long* buf, unsigned long len ,int status)//0_read  1_write
{
    int i = 0;
    int fd, ret;
    void *map_base, *virt_addr;
    off_t addr = readAddr;
    unsigned long page_addr = addr & ~MAP_MASK;
    if ((fd = open("/dev/mem", O_RDWR | O_SYNC)) == -1)
    {
        fprintf(stderr, "Error at line %d, file %s (%d) [%s]\n", __LINE__, __FILE__, errno, strerror(errno));
        return 0;
    }

    /* Map one page */ //将内核空间映射到用户空间
    map_base = mmap(0, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, addr & ~MAP_MASK);
    if(map_base == (void *) -1)
    {
        fprintf(stderr, "Error at line %d, file %s (%d) [%s]\n", __LINE__, __FILE__, errno, strerror(errno));
        close(fd);
        return 0;
    }
    for (i = 0; i < len; i++)
    {
        // 翻页处理
        if((addr & ~MAP_MASK) != page_addr)
        {
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

        virt_addr = map_base + (addr & MAP_MASK);	// 将内核空间映射到用户空间操作
        if(status == 0){
            buf[i] = *((unsigned long *) virt_addr);	// 读取数据
        }
        if(status == 1){
            *((unsigned long *)virt_addr) = buf[i];
        }
        addr += 4;
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

int ddr_transfer(void *map_base0)
{
    //unsigned int *map_base0;

    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd < 0) {
        printf("can not open /dev/mem \n");
        return (-1);
    }
    printf("/dev/mem is open \n");

    map_base0 = mmap(NULL, DATA_LEN * 4, PROT_READ | PROT_WRITE, MAP_SHARED, fd, BRAM_CTRL_0);
    if (map_base0 == (void *)-1) {
        printf("NULL pointer\n");
    }
    else {
        printf("mmap successful\n");
    }

    unsigned long addr;
    unsigned int content;

    printf("\nwrite data to bram\n");
    addr = (unsigned long)(map_base0);
    content = 0x12345678;
    *((unsigned long *)map_base0) = content;    //using data format tansfer
    printf("address: 0x%lx data_write: 0x%x\t\t\n",addr, content);

    close(fd);
    //munmap(map_base0, DATA_LEN);

    return 0;
}

int action_elf(string command){
    int result = system(command.c_str());
    if(result == -1){
        std::cerr << "system() failed" << std::endl;
        return -1;
    }
    //quit system command stage.
    if(WIFEXITED(result)){
        int exit_status = WEXITSTATUS(result);  //get quit stage.
        std::cout <<".elf file qxited with status" <<exit_status << std::endl;
    }else{
        std::cout <<".elf file did not exit."  << std::endl;
    }
    return 0;
}
