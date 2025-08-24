#include "stdio.h"
#include "unistd.h"
#include "sys/types.h"
#include "sys/stat.h"
#include "fcntl.h"
#include "stdlib.h"
#include "string.h"
#include "linux/ioctl.h"

#define CLOSE_CMD   (_IO(0XEF, 0X1))
#define OPEN_CMD    (_IO(0XEF, 0X2))
#define SETPERIOD_CMD   (_IO(0XEF, 0X3))

int main(int argc, char *argv[])
{
    int fd, ret;
    unsigned int cmd;
    unsigned int arg;
    char *filename;
    unsigned char str[100];

    if(argc != 2) {
        printf("Error Usage!\r\n");
        return -1;
    }
    filename = argv[1];

    fd = open(filename, O_RDWR);
    if(fd < 0) {
        printf("can't open %s file!\r\n", filename);
        return -1;
    }

    while(1) {
        printf("Input CMD:");
        /*
        * @description:scanf的返回值
        * @return 1:成功读取并匹配了一个整数，存储在 arg 中
        * @return 0:输入与格式不匹配
        * @return EOF:遇到输入结束或错误条件
        */
        ret = scanf("%d", &cmd);
        if(ret != 1) {
            gets(str);
        }

        if(cmd == 1) {
            cmd = CLOSE_CMD;
        } else if (cmd == 2) {
            cmd = OPEN_CMD;
        } else if (cmd == 3) {
            cmd = SETPERIOD_CMD;
            printf("Input Timer Period:");
            ret = scanf("%d", &arg);
            if(ret != 1) {
                gets(str);
            }
        }
        ioctl(fd, cmd, arg);
    }
    close(fd);
}
