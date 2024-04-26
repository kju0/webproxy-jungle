#include "csapp.h"

int main(void) {
    char *buf, *p, *method;
    char arg1[MAXLINE], arg2[MAXLINE], content[MAXLINE];
    int num1 = 0, num2 = 0;

    // 쿼리 스트링을 가져와서 두 숫자를 추출
    if ((buf = getenv("QUERY_STRING")) != NULL) {
        p = strchr(buf, '&');
        *p = '\0';
        strcpy(arg1, buf);
        strcpy(arg2, p+1);
        num1 = atoi(arg1);
        num2 = atoi(arg2);
    }
    method = getenv("REQUEST_METHOD");
    if (method != NULL){
      printf("method의 값: %s\n", method);
    }
    // 결과를 생성
    sprintf(content, "Welcome to add.com: ");
    sprintf(content, "%sTHE Internet addition portal.\r\n<p>", content);
    sprintf(content, "%sThe answer is: %d + %d = %d\r\n<p>", content, num1, num2, num1 + num2);
    sprintf(content, "%sThanks for visiting!\r\n", content);

    // HTTP 응답 헤더를 작성
    printf("HTTP/1.0 200 OK\r\n");
    printf("Server: Tiny Web Server\r\n");
    printf("Content-Type: text/html\r\n");
    printf("Content-Length: %d\r\n\r\n", (int)strlen(content));

    

    if (strcasecmp(method,"GET") == 0){
      printf("%s", content);
    }
    


    fflush(stdout);
    exit(0);
}
