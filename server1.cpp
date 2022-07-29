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
int createsocket(int sockfd);

int bindsocket(int sockfd, struct sockaddr *addr, socklen_t addrlen);

int listensocket(int sockfd, int max_events);

int acceptsocket(int sockfd, struct sockaddr *addr, socklen_t *addrlen);

void cf(io_context_t ctx, struct iocb *iocb, long res, long res2);

struct epoll_event *events;
struct epoll_event ev;
struct io_event eve[64] = {};

int main(int argc, char *argv[])
{
  int evfd, fp;
  int sockfd;
  io_context_t ctx;
  char recvname[20];
  char buf[1024];
  struct iocb cb;
  struct iocb *pcbs[1];
  struct timespec ts;
  int n;
  unit64_t ready;
  struct io_event *events_ret;

  struct sockaddr_in ser;
  ser.sin_family = AF_INET;
  ser.sin_port = htons(atoi("8888"));
  ser.sin_addr.s_addr = inet_addr("172.20.38.143");

  createsocket(sockfd);
  socklen_t len = sizeof(struct sockaddr_in);
  bindsocket(sockfd, (struct sockaddr *)&ser, len);
  listensocket(sockfd, MAX_EVENTS);
  acceptsocket(sockfd, (struct sockaddr *)&ser, &len);
  int epoll_fd = epoll_create(MAX_EVENTS);
  ev.events = EPOLLIN;
  ev.data.fd = sockfd;
  int ret1 = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sockfd, &ev);
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
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, evfd, &ev) != 0)
    {
      perror("evfd epoll_ctl error");
      exit(6);
    }
    ctx = 0;
    memset(&ctx,0,sizeof(ctx))
    int errcode = io_setup(1024,&ctx);
    if(errcode != 0)
    {
      perror("open");
      exit(7);
    }
    int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
    if (nfds == -1)
    {
      perror("epoll_wait error");
      exit(EXIT_FAILURE);
    }
    printf("nfds:%d\n", nfds);

    for (int i = 0; i < nfds; ++i)
    {
      if (events[i].data.fd == sockfd)
      {
        int confd = acceptsocket(sockfd, (struct sockaddr *)&ser, &len);
        ev.events = EPOLLIN | EPOLLET;
        ev.data.fd = confd;
        int ret2 = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, confd, &ev);
        if (ret2 == -1)
        {
          perror("epoll_ctl  sockfd error");
          return -1;
        }
      }
      else if (events[i].events & EPOLLIN)
      {
        int r_fd = recv(sockfd, buf, 0, sizeof(buf));
        if (r_fd == -1)
        {
          perror("recv error");
          return -1;
        }
        if (r_fd == 0)
        {
          perror("退出");
          ev.events = EPOLLIN;
          ev.data.fd = events[i].data.fd;
          epoll_ctl(epoll_fd, EPOLL_CTL_DEL, ev.data.fd, &ev);
          close(ev.data.fd);
        }
        else
        {
          strncpy(recvname, buf, strlen(buf));
          fp = open(buf, O_WRONLY, 0664);
          errcode = posix_memalign((void**)&buf,512,1024);
          io_prep_pwrite(&cb, fp, buf, 1024, 0);
          io_set_eventfd(&cb, evfd);
          io_set_callback(&cb, cf);
          pcbs[0] = &cb;
          if (io_submit(ctx, 1, pcbs) != 1)
          {
            perror("io_submit");
            exit(8);
          }
          send(events[i].data.fd, buf, sizeof(buf), 0);
        }
      }
      else if (events[i].events & EPOLLOUT)
      {
        fp = open(buf, O_RDONLY | O_CREAT, 0664);
        errcode = posix_memalign((void**)&buf,512,1024);
        io_prep_pread(&cb, fp, buf, 1024, 0);
        io_set_eventfd(&cb, evfd);
        io_set_callback(&cb, cf);
        pcbs[0] = &cb;
        if (io_submit(ctx, 1, pcbs) != 1)
        {
          perror("io_submit");
          exit(11);
        }
        n = read(evfd, &ready, sizeof(ready));
        if (n != 8)
        {
          perror("read error");
          exit(12);
        }
        ts.tv_sec = 0;
        ts.tv_nsec = 0;
        events_ret = (struct io_event *)malloc(sizeof(struct io_event) * 32);
        n = io_getevents(ctx, 1, 32, events_ret, &ts);

        printf("log: %lu events are ready, get %d events\n", ready, n);

        ((io_callback_t)(events_ret[0].data))(ctx, events_ret[0].obj, events_ret[0].res, events_ret[0].res2);
      }
      memset(&buf, 0x00, 1023);
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

int createsocket(int sockfd)
{
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0)
  {
    perror("sockfd error");
    return -1;
  }
}

int bindsocket(int sockfd, struct sockaddr *addr, socklen_t addrlen)
{
  if (bind(sockfd, (struct sockaddr *)&addr, addrlen) < 0)
  {
    perror("bind error");
    exit(EXIT_FAILURE);
  }
}

int listensocket(int sockfd, int max_events)
{
  if (listen(sockfd, max_events) < 0)
  {
    perror("listen error");
    exit(EXIT_FAILURE);
  }
}

int acceptsocket(int sockfd, struct sockaddr *addr, socklen_t *addrlen)
{
  if (accept(sockfd, (struct sockaddr *)addr, addrlen) < 0)
  {
    perror("accept error");
    exit(EXIT_FAILURE);
  }
}

void cf(io_context_t ctx, struct iocb *iocb, long res, long res2)
{
  printf("can read %lu bytes, and in fact has read %ld bytes\n", iocb->u.c.nbytes, res);
  printf("the content is :\n%lu", iocb->u.c.buf);
}
