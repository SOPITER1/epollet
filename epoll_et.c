#include<stdio.h>
#include<sys/socket.h>
#include<stdlib.h>
#include<netinet/in.h>
#include<unistd.h>
#include<fcntl.h>
#include<string.h>
#include<sys/epoll.h>
#include<errno.h>
typedef struct client
{   
    int fd;
    char *buff;
    ssize_t total;
    struct client* last;
    struct client* next;
}client;

typedef struct 
{
    client*hread;
    client*tail;
    int count;
}clientnext;

#define Maxlist 1024
int accept_ctladd(int serverfd,int epfd)
{
                struct sockaddr_in addr;
                memset(&addr,0,sizeof(addr));
                socklen_t addrcount=sizeof(addr);
                int clientfd=accept(serverfd,(struct sockaddr*)&addr,&addrcount);
                if (clientfd==-1)
                {
                    perror("accept");
                    return -1;
                }
                int flags=fcntl(clientfd,F_GETFL,0);
                fcntl(clientfd,F_SETFL,flags|O_NONBLOCK);
                struct epoll_event ev;
                ev.data.fd=clientfd;
                ev.events=EPOLLIN|EPOLLET;
                if (epoll_ctl(epfd,EPOLL_CTL_ADD,clientfd,&ev)==-1)
                {
                    perror("epoll_ctl");
                    close(clientfd);
                    return -1;
                }
                printf("客户端【%d】已连接\n",clientfd);
                return clientfd;
}
void createnext(int clientfd,clientnext* clientnext)
{
    client* ct=malloc(sizeof(client));
    memset(ct,0,sizeof(client));
    ct->buff=malloc(sizeof(char)*Maxlist);
    ct->last=NULL;
    ct->next=NULL;
    ct->total=0;
    memset(ct->buff,0,sizeof(char)*Maxlist);
    ct->fd=clientfd;
    if (clientnext->hread==NULL)
    {
        clientnext->hread=ct;
        clientnext->tail=ct;
        clientnext->count=1;
    }
    else
    {
        clientnext->tail->next=ct;
        client* last=clientnext->tail;
        ct->last=last;
        clientnext->tail=ct;
        clientnext->count++;
    }
    printf("客户端【%d】已经加入队列\n",ct->fd);
    return ;
}
client* Gettarget(int clientfd,clientnext* ctn)
{
    if (ctn->hread==NULL)
    {
        printf("当前列表并为有用户在线\n");
        return  ctn->hread;
    }
    if (ctn->hread->fd==clientfd)
    {
        return ctn->hread;
    }
    client *ct=ctn->hread->next;
    while (ct!=NULL)
    {
        if (ct->fd==clientfd)
        {
            return ct;
        }
        ct=ct->next;
    }
}
int deleepoll(int epfd,int clientfd)
{
    if (epoll_ctl(epfd,EPOLL_CTL_DEL,clientfd,NULL))
    {
        perror("epoll_ctl");
        return -1;
    }
    
}
void delenext(clientnext*ctn,client* ct)
{
    if (ctn==NULL||ct==NULL)
    {
        return ;
    }
    if (ct->last==NULL&&ct->next==NULL)
    {
        ctn->hread=NULL;
        ctn->tail=NULL;
        ctn->count=0;
    }
    else if(ct->last==NULL)
    {
        ct->next->last=NULL;
        ctn->hread=ct->next;
    }else if (ct->next==NULL)
    {
        ct->last->next=NULL;
        ctn->tail=ct->last;
    }else
    {
        ct->last->next=ct->next;
        ct->next->last=ct->last;
    }
    ctn->count--;
    free(ct->buff);
    close(ct->fd);
    free(ct);
}
void send_string(clientnext* ctn,char*msg,ssize_t* total)
{
    char *p=memchr(msg,'\n',*total);
    int mchr=p?(int)(p-msg):-1;
    if(ctn->hread==NULL)
    {
        printf("当前不存在用户\n");
        return ;
    }
    int count=0;
    client*ct=ctn->hread;
    while (count!=ctn->count)
    {
        msg[(*total)]='\0';
        send(ct->fd,msg,mchr+1,0);
        ct=ct->next;
        count++;
    }
    ssize_t reman=(*total)-mchr-1;
    memmove(msg,msg+mchr+1,reman);
    *total=reman;
}
int main(int argc, char const *argv[])
{
    int serverfd=socket(AF_INET,SOCK_STREAM,0);
    if (serverfd==-1)
    {
        perror("socket");
        return -1;
    }
    struct sockaddr_in addr;
    memset(&addr,0,sizeof(addr));
    addr.sin_family=AF_INET;
    addr.sin_port=htons(8080);
    addr.sin_addr.s_addr=INADDR_ANY;
    int opt=1;
    if (setsockopt(serverfd,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt))==-1)
    {
        perror("sersockopt");
        close(serverfd);
        return -1;
    }
    if (bind(serverfd,(struct sockaddr*)&addr,sizeof(addr))==-1)
    {
        perror("bind");
        close(serverfd);
        return -1;
    }
    if (listen(serverfd,5)==-1)
    {
        perror("listen");
        close(serverfd);
        return -1;
    }
    int epfd=epoll_create(1);
    struct epoll_event ev;
    memset(&ev,0,sizeof(ev));
    ev.data.fd=serverfd;
    ev.events=EPOLLIN;
    if (epoll_ctl(epfd,EPOLL_CTL_ADD,serverfd,&ev))
    {
        perror("epoll_ctl");
        close(serverfd);
        return -1;
    }
    struct epoll_event events[Maxlist];
    clientnext clientnext;
    while (1)
    {
        int nready=epoll_wait(epfd,events,Maxlist,-1);
        for (int i = 0; i < nready; i++)
        {
            int fd=events[i].data.fd;
            if (fd==serverfd)
            {
                int clientfd=accept_ctladd(fd,epfd);
                createnext(clientfd,&clientnext);
            }
            else
            {
                client* ct=Gettarget(fd,&clientnext);
                while (1)
                {
                    int n=recv(fd,ct->buff+ct->total,sizeof(char)*Maxlist-ct->total-1,0);
                    if (n<=0)
                    {
                        if (n==0)
                        {
                            printf("客户端【%d】连接已断开\n",fd);
                            deleepoll(epfd,fd);
                            delenext(&clientnext,ct);
                            break;
                        }
                        if (n==-1)
                        {
                            if (errno==EAGAIN||errno==EWOULDBLOCK)
                            {
                                break;
                            }
                            else
                            {
                                perror("recv");
                                deleepoll(epfd,fd);
                                delenext(&clientnext,ct);
                                break;
                            }
                        }
                        
                    }
                    else
                    {
                        ct->total+=n;
                        while(memchr(ct->buff,'\n',ct->total)!=NULL)
                        {
                            send_string(&clientnext,ct->buff,&ct->total);
                        }
                    }
                    
                }
                
            }
        }
        
    }
    close(serverfd);
    return 0;
}
