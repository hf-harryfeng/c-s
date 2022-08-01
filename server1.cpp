#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdint.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <libaio.h>

#define FILEPATH "./aio.txt"

#define _GNU_SOURCE
#define __STDC_FORMAT_MACROS
#define MAX_EVENTS 10

typedef unsigned long int unit64_t;

void cf(io_context_t ctx, struct iocb *iocb, long res, long res2);

struct epoll_event *events;
struct epoll_event ev; /*定义epoll对象*/
struct io_event eve[64] = {};

int main(int argc, char *argv[])
{
  int evfd, fp;
  int sockfd;
  io_context_t ctx; /*异步io fd*/
  char buf[1024] = {0};
  char filename[10] = "test.txt";
  struct iocb cb;       /*一个异步io读取文件事件*/
  struct iocb *pcbs[1]; /*io_submit参数，包含cb*/
  struct timespec ts;   /*io_getevents定义定时器*/
  int n;
  unit64_t ready;              /*已完成的异步io事件*/
  struct io_event *events_ret; /*io_getevents参数*/

  struct sockaddr_in ser;
  ser.sin_family = AF_INET;
  ser.sin_port = htons(atoi("8888"));
  ser.sin_addr.s_addr = inet_addr("172.20.38.142");
  /*绑定通讯地址*/
  sockfd = socket(AF_INET, SOCK_STREAM, 0); /*创建socket套接字*/
  if (sockfd < 0)
  {
    perror("sockfd error");
    return -1;
  }
  socklen_t len = sizeof(struct sockaddr_in);
  if (bind(sockfd, (struct sockaddr *)&ser, len) < 0) /*绑定套接字地址*/
  {
    perror("bind error");
    exit(EXIT_FAILURE);
  }
  if (listen(sockfd, MAX_EVENTS) < 0) /*监听socket套接字，MAX_EVENTS监听最大数*/
  {
    perror("listen error");
    exit(EXIT_FAILURE);
  }
  int epoll_fd = epoll_create(MAX_EVENTS); /*初始化epoll*/
  ev.events = EPOLLIN;                     /*epoll 事件为可读*/
  ev.data.fd = sockfd;
  int ret1 = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sockfd, &ev); /*添加ev.data.fd对应的事件进表*/
  if (ret1 == -1)
  {
    perror("epoll_ctl  sockfd error");
    return -1;
  }

  while (1)
  {
    evfd = eventfd(0, 0);
    if (evfd < 0)
    {
      perror("eventfd");
      exit(5);
    }
    ev.events = EPOLLIN | EPOLLET;
    ev.data.ptr = NULL;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, evfd, &ev) != 0) /*初始化eventfd描述符，并将其添加进epoll*/
    {
      perror("evfd epoll_ctl error");
      exit(6);
    }
    ctx = 0; /*初始化异步io上下文*/
    memset(&ctx, 0, sizeof(ctx));
    int errcode = io_setup(1024, &ctx);
    if (errcode != 0)
    {
      perror("open");
      exit(7);
    }
    int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1); /*调用epoll_wait等待事件发生*/
    if (nfds <= 0)
    {
      if (errno != EINTR)
      {
        perror("epoll_wait error");
        exit(8);
      }
    }
    else
      continue;
    printf("nfds:%d\n", nfds);

    for (int i = 0; i < nfds; ++i)
    {
      if (events[i].data.fd == sockfd) /*如果有新的sockfd连接*/
      {
        int confd = accept(sockfd, (struct sockaddr *)&ser, &len);
        ev.events = EPOLLIN | EPOLLET;
        ev.data.fd = confd;
        int ret2 = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, confd, &ev); /*初始化accept进行接受，并将其添加进epoll事件*/
        if (ret2 == -1)
        {
          perror("epoll_ctl  sockfd error");
          return -1;
        }
      }
      else if (events[i].events & EPOLLIN) /*如果是已经连接的用户，并且接收数据，那么进行读入*/
      {
        fp = open(filename, O_RDONLY | O_CREAT, 0664); /*创建接收的新文件*/
        io_prep_pread(&cb, fp, buf, 1024, 0);          /*将buf内容异步读入fp指向文件请求*/
        io_set_eventfd(&cb, evfd);                     /*将eventfd与cb上下文进行绑定*/
        io_set_callback(&cb, cf);
        pcbs[0] = &cb;
        if (io_submit(ctx, 1, pcbs) != 1) /*异步请求处理*/
        {
          perror("io_submit");
          exit(10);
        }
        n = read(evfd, &ready, sizeof(ready));
        if (n != 8)
        {
          perror("read error");
          exit(11);
        }
        memset(&buf, 0x00, 1023);
      }
      else if (events[i].events & EPOLLOUT) /*如果是已经连接的用户，并且有数据发送*/
      {
        int r_fd = recv(sockfd, buf, 0, sizeof(recvname)); /*接收客户端发送的文件名*/

        fp = open(buf, O_WRONLY, 0664); /*打开要发送的文件*/
        memset(&buf, 0x00, 1023);
        io_prep_pwrite(&cb, fp, buf, 1024, 0); /*将fp指向文件内容异步写入buf请求*/
        io_set_eventfd(&cb, evfd);             /*将eventfd与cb上下文进行绑定*/
        io_set_callback(&cb, cf);
        pcbs[0] = &cb;
        if (io_submit(ctx, 1, pcbs) != 1) /*异步请求处理*/
        {
          perror("io_submit");
          exit(9);
        }
        send(sockfd, buf, sizeof(buf), 0);
        ev.data.fd = sockfd;
        ev.events = EPOLLIN | EPOLLET;
        epoll_ctl(epoll_fd, EPOLL_CTL_MOD, sockfd, &ev);
      }
      ts.tv_sec = 0;    /*获取异步请求处理结果计时器*/
      ts.tv_nsec = 0;
      events_ret = (struct io_event *)malloc(sizeof(struct io_event) * 32);
      n = io_getevents(ctx, 1, 32, events_ret, &ts);/*获取异步请求处理结果*/

      printf("log: %lu events are ready, get %d events\n", ready, n);

      ((io_callback_t)(events_ret[0].data))(ctx, events_ret[0].obj, events_ret[0].res, events_ret[0].res2);
    }
  }
  close(epoll_fd);
  io_destroy(ctx);
  free(buf);
  close(fp);
  close(evfd);
  return 0;
  close(sockfd);
}

void cf(io_context_t ctx, struct iocb *iocb, long res, long res2)
{
  printf("can read %lu bytes, and in fact has read %ld bytes\n", iocb->u.c.nbytes, res);
  printf("the content is :%lu", iocb->u.c.buf);
}
