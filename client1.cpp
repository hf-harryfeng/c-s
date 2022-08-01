#include <stdio.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <getopt.h>

#define MAX_EVENTS 10
#define FILEPATH "./aio.txt"

int main(int argc, char **argv)
{
    char buf[1024] = {0};
    int fp, ret, ret2, ret3;
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        perror("sockfd error");
        return -1;
    }
    socklen_t len = sizeof(struct sockaddr_in);

    struct sockaddr_in ser;
    ser.sin_family = AF_INET;
    ser.sin_port = htons(atoi("8888"));
    ser.sin_addr.s_addr = inet_addr("172.20.38.142");

    if (connect(sockfd, (struct sockaddr *)&ser, len) < 0)
    {
        perror("connect error");
        exit(EXIT_FAILURE);
    }

    char *l_opt_arg;
    const char *short_options = "g:p:";

    struct option long_options[] = {
        {"get", 1, NULL, 'g'},
        {"pull", 1, NULL, 'p'},
        {0, 0, 0, 0}};
    int C;
    while (C = getopt_long(argc, argv, short_options, long_options, NULL) != -1)
    {
        switch (C)
        {
            l_opt_arg = optarg; /*l_opt_arg指向-g或者-p后所跟参数（文件名）*/
        case 'g':
            ret = send(sockfd, l_opt_arg, sizeof(l_opt_arg), 0); /*发送要获取的文件名*/
            fp = open(l_opt_arg, O_WRONLY | O_CREAT, 0664);      /*创建一个需要get的新文件*/
            if (fp < 0)
            {
                printf("File :\t%s can not open to write\n", l_opt_arg);
                exit(1);
            }

            while (ret2 > 0) /*循环接受数据并写入文件*/
            {
                ret2 = recv(sockfd, buf, sizeof(buf), 0);
                if (write(fp, buf, sizeof(buf)) == -1)
                {
                    printf("File :%s\t write failed\n", l_opt_arg);
                    break;
                }
                bzero(buf, sizeof(buf));
            }
            printf("this file: %s\t get success", l_opt_arg);
            break;
        case 'p':
            fp = open(l_opt_arg, O_RDONLY); /*打开要发送的文件*/
            if (fp < 0)
            {
                printf("File :%s\t can not open to write\n", l_opt_arg);
                exit(1);
            }

            while (ret3 > 0) /*循环读取文件并发送*/
            {
                ret3 = fread(buf, sizeof(char), sizeof(buf), 0);
                if (send(sockfd, buf, sizeof(buf), 0) < 0)
                {
                    printf("File:%s\t read Failed\n", l_opt_arg);
                    exit(1);
                }
                bzero(buf, sizeof(buf));
            }
            printf("this file:%s\t pull success", l_opt_arg);
            break;
        }
    }
}
