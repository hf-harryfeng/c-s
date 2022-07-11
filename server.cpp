#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<errno.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<unistd.h>
#include<sys/stat.h>
#include<sys/types.h>
#include<fcntl.h>
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
    if(bind(sockfd,(struct sockaddr*)&ser,len)<0)
    {
        perror("bind error");
        close(sockfd);
        return -1;
    }

   
    if(listen(sockfd,5)<0)
    {
        perror("listen error");
        close(sockfd);
        return -1;
    }
    while(1)
    {
        struct sockaddr_in cli;
       
        int n_sockfd=accept(sockfd,(struct sockaddr*)&cli,&len);
        if(n_sockfd<0){
            perror("accept error");
            continue;
        }
       
        while(1){
	    char buf[1024] = {0};		  		 
            int fd_test = open("../fhr/test.txt",O_RDONLY);
            {
                if (fd_test < 0)
                {
                    perror("open test.txt");
                    return -1;
                }
            }
                int len = read(fd_test,buf,sizeof(buf));
		{
		if (len < 0)
		{
		perror("read len");
		return -1;
		}
	}
            close(fd_test);
	    send(n_sockfd,buf,sizeof(buf),0);  
}
            close(n_sockfd);
    }
}