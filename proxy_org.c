#include <stdio.h>
#include <pthread.h>
#include "csapp.h"
/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/*
 * 
 */

typedef void *(func) (void *);
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static const char *conn_hdr = "Connection: close\r\n";
static const char *prox_hdr = "Proxy-Connection: close\r\n";
static const char *host_hdr_format = "Host: %s\r\n";
static const char *requestlint_hdr_format = "GET %s HTTP/1.0\r\n";
static const char *endof_hdr = "\r\n";

static const char *connection_key = "Connection";
static const char *user_agent_key = "User-Agent";
static const char *proxy_connection_key = "Proxy-Connection";
static const char *host_key = "Host";

// commnuication from client to server
void doit(int connfd);
// parsing the uri that client requests
void parse_uri(char *uri,char *hostname, char *path, int *port);
void build_http_header(char *http_header, char *hostname, char *path, int port, rio_t *client_rio);
int connect_endServer(char *hostname, int port);
void *thread(void *vargp);

int main(int argc, char **argv)
{
    pthread_t tid;
    int listenfd, *connfd;
    socklen_t  clientlen;
    char hostname[MAXLINE], port[MAXLINE];

    struct sockaddr_storage clientaddr;/*generic sockaddr struct which is 28 Bytes.The same use as sockaddr*/

    // port number가 argument로 입력되지 않은 경우 error반환
    if(argc != 2){
        fprintf(stderr,"usage :%s <port> \n",argv[0]);
        exit(1);
    }

    // get listenfd
    listenfd = Open_listenfd(argv[1]);
    while(1) {
        clientlen = sizeof(clientaddr);
        connfd = Malloc(sizeof(int));
        *connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);

        // hostname과 portnumber string으로 반환
        Getnameinfo((SA*)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
        printf("Accepted connection from (%s %s).\n", hostname, port);

        Pthread_create(&tid, NULL, thread, connfd);
    }
    return 0;
}

void * thread(void *vargp)
{
    int connfd = *((int *)vargp);
    Pthread_detach(pthread_self());
    Free(vargp);
    doit(connfd);
    Close(connfd);
    return NULL;
}


void doit(int connfd)
{
    // proxy 뒤에 존재하는 end server
    int end_serverfd;
    int port;

    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char endserver_http_header[MAXLINE];
    char hostname[MAXLINE], path[MAXLINE];
    /*rio is client's rio,server_rio is endserver's rio*/
    rio_t rio, server_rio;

    Rio_readinitb(&rio, connfd);
    // read the client's rio into buffer
    Rio_readlineb(&rio, buf, MAXLINE);
    sscanf(buf,"%s %s %s", method, uri, version);
    // request의 method가 GET이 아니면 error 처리
    if(strcasecmp(method, "GET")){
        printf("Proxy does not implement the method");
        return;
    }

    parse_uri(uri, hostname, path, &port);
    printf("%s\n", path);
    /*build the http header which will send to the end server*/
    build_http_header(endserver_http_header, hostname, path, port, &rio);

    /*connect to the end server*/
    // end_serverfd = connect_endServer(hostname, port, endserver_http_header);
    end_serverfd = connect_endServer(hostname, port);
    if(end_serverfd < 0) {
        printf("connection failed\n");
        return;
    }

    Rio_readinitb(&server_rio, end_serverfd);
    Rio_writen(end_serverfd, endserver_http_header, strlen(endserver_http_header));

    /*receive message from end server and send to the client*/
    size_t n;
    while((n=Rio_readlineb(&server_rio,buf,MAXLINE))!=0)
    {
        printf("proxy received %d bytes,then send\n",n);
        Rio_writen(connfd,buf,n);
    }
    Close(end_serverfd);
}

//http_header 인자에 만들어서 반환
void build_http_header(char *http_header, char *hostname, char *path, int port, rio_t *client_rio)
{
    char buf[MAXLINE], request_hdr[MAXLINE], other_hdr[MAXLINE], host_hdr[MAXLINE];
    // request_hdr에 reqquestlint_hdr_format을 담음(path인자는 reqquestlint_hdr_format에 들어갈 값)
    // path는 request source 경로
    sprintf(request_hdr, requestlint_hdr_format, path);
    /*get other request header for client rio and change it */
    while(Rio_readlineb(client_rio, buf, MAXLINE) > 0)
    {
        // 읽어들인 값(buf)가 /r/n이면 break
        if(strcmp(buf, endof_hdr) == 0)
        {
            break;
        }

        // host header값 만들어주기
        if(!strncasecmp(buf, host_key, strlen(host_key)))/*Host:*/
        {
            // buf를 host_hdr에서 지정한 위치로 복사
            strcpy(host_hdr, buf);
            continue;
        }

        // host header 이외의 header 만들어주기
        if(!strncasecmp(buf, connection_key, strlen(connection_key))
                &&!strncasecmp(buf, proxy_connection_key, strlen(proxy_connection_key))
                &&!strncasecmp(buf, user_agent_key, strlen(user_agent_key)))
        {
            strcat(other_hdr, buf);
        }
    }

    // request header에 host header가 없다면 hostname으로 만들어주기
    if(strlen(host_hdr) == 0)
    {
        sprintf(host_hdr, host_hdr_format, hostname);
    }
    // 완전체 만들어주기
    sprintf(http_header, "%s%s%s%s%s%s%s", request_hdr, host_hdr, conn_hdr, prox_hdr, user_agent_hdr, other_hdr, endof_hdr);
    return ;
}

/*Connect to the end server*/
// inline int connect_endServer(char *hostname, int port, char *http_header){
int connect_endServer(char *hostname, int port){
    char portStr[100];
    // portstr에 port 넣어주기
    sprintf(portStr, "%d", port);
    // 해당 hostname과 portStr로 end_server에게 가는 요청만들어주기
    return Open_clientfd(hostname, portStr);
}

/*parse the uri to get hostname,file path ,port*/
void parse_uri(char *uri,char *hostname,char *path,int *port)
{
    *port = 80;
    // uri에서 "//"의 첫 번째 표시 시작 위치에 대한 포인터를 리턴
    char* pos = strstr(uri, "//");

    pos = pos != NULL? pos+2:uri;

    char*pos2 = strstr(pos, ":");
    if(pos2 != NULL)
    {
        *pos2 = '\0';
        sscanf(pos, "%s", hostname);
        sscanf(pos2+1, "%d%s", port, path);
    }
    else
    {
        pos2 = strstr(pos,"/");
        if(pos2!=NULL)
        {
            *pos2 = '\0';
            sscanf(pos,"%s",hostname);
            *pos2 = '/';
            sscanf(pos2,"%s",path);
        }
        else
        {
            sscanf(pos,"%s",hostname);
        }
    }
    return;
}