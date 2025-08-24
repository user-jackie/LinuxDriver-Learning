#include "stdio.h"
#include "unistd.h"
#include "sys/types.h"
#include "sys/stat.h"
#include "fcntl.h"
#include "stdlib.h"
#include "string.h"

#define BEEPOFF     0
#define BEEPON      1

/* 此文件用于与linux终端(即用户)交互，将命令陷入到内核中 */
/* open close write read函数 */
int main(int argc, char *argv[])
{
    int fd, retvalue;
    char *filename;
    unsigned char databuf[1];

    if(argc != 3) {
        printf("Error Usage!\r\n");
        return -1;
    }

    filename = argv[1];

    fd = open(filename, O_RDWR);
    if(fd < 0) {
        printf("open %s failed!\r\n", filename);
        return -1;
    }

    databuf[0] = atoi(argv[2]);
    
    retvalue = write(fd, databuf, sizeof(databuf));
    if(retvalue < 0) {
        printf("BEEP Control Failed!\r\n");
        close(fd);
        return -1;
    }

    retvalue = close(fd);
    if(retvalue < 0) {
        printf("file %s close failed!\r\n", argv[1]);
        return -1;
    }

    return 0;
}
