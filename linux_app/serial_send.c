/*
 * serial_send.c - Linux 串口发送示例程序
 *
 * 功能：打开 /dev/ttyACM0（常见于 ESP32 等 USB 串口设备），
 *       以 115200 8N1 配置每秒发送一行 "Hello ESP32 <时间戳>"，
 *       并持续读取串口返回数据并打印到终端。
 * 编译：gcc -o serial_send serial_send.c
 * 运行：./serial_send
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <time.h>
#include <sys/select.h>

int main() {
    /* 打开串口设备（ESP32 等 USB 转串口通常为 ttyACM0） */
    int fd = open("/dev/ttyACM0", O_RDWR | O_NOCTTY);
    if (fd < 0) {
        perror("open failed");
        return 1;
    }

    /* 获取当前终端属性并在此基础上修改 */
    struct termios tty;
    tcgetattr(fd, &tty);

    /* 设置波特率为 115200 */
    cfsetospeed(&tty, B115200);
    cfsetispeed(&tty, B115200);

    /* 串口参数：本地连接、使能接收、8 数据位、无校验、1 停止位、无硬件流控 */
    tty.c_cflag |= (CLOCAL | CREAD);
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;
    tty.c_cflag &= ~PARENB;
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CRTSCTS;

    /* 原始模式：关闭规范输入/输出与本地回显等 */
    tty.c_lflag = 0;
    tty.c_oflag = 0;
    tty.c_iflag = 0;

    tcsetattr(fd, TCSANOW, &tty);

    printf("串口 /dev/ttyACM0 已打开，115200 8N1。按 Ctrl+C 退出。\n");
    fflush(stdout);

    time_t last_send = time(NULL);
    char rbuf[256];

    while (1) {
        struct timeval tv = { .tv_sec = 0, .tv_usec = 100000 }; /* 100ms */
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(fd, &rfds);

        int ret = select(fd + 1, &rfds, NULL, NULL, &tv);
        if (ret > 0 && FD_ISSET(fd, &rfds)) {
            ssize_t n = read(fd, rbuf, sizeof(rbuf) - 1);
            if (n > 0) {
                rbuf[n] = '\0';
                printf("[收到] %s", rbuf);
                fflush(stdout);
            }
        }

        time_t now = time(NULL);
        if (now - last_send >= 1) {
            char buffer[128];
            snprintf(buffer, sizeof(buffer),
                     "Hello ESP32 %ld\r\n", now);
            ssize_t n = write(fd, buffer, strlen(buffer));
            printf("[%ld] 已发送 %zd 字节: %s", (long)now, n, buffer);
            fflush(stdout);
            last_send = now;
        }
    }

    close(fd);
    return 0;
}
