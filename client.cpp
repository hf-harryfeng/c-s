#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<errno.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<unistd.h>
int main()
{
    
    int sockfd=socket(AF_INET,SOCK_STREAM,0);
    if(sockfd<0){
        perror("socket error");
        return -1;
    }
    struct sockaddr_in ser;
    ser.sin_family=AF_INET;
    ser.sin_port=htons(atoi("8080"));
    ser.sin_addr.s_addr=inet_addr("172.20.38.143");
    
    socklen_t len=sizeof(struct sockaddr_in);
    if(connect(sockfd,(struct sockaddr*)&ser,len)<0)
    {
        perror("connect error");
        return -1;
    }
    
    while(1)
    {
	    char buf[1024] = {0};
            int ret=recv(sockfd,buf,1023,0);
            if(ret<0){
                perror("recv error");
		continue;
            }
            printf("server :%s",buf); 
	    break;
    }
close(sockfd);
}
