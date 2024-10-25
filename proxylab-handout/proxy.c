#include <stdio.h>
#include <stdlib.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";


void *serve_client(void *connfd);

int parse_request(rio_t *rp, char *req_buf);

void response(rio_t *rp, char *msg);

void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);

int main(int argc, char *argv[])
{   
    if (argc != 2) {
        printf("Usage: ./proxy <port number>\n");
        exit(0);    
    }

    int listenfd, connfd;
    struct sockaddr_storage clientaddr;
    socklen_t clientaddr_len = sizeof(struct sockaddr_storage);
    char hostname[MAXLINE], port[MAXLINE];

    listenfd = Open_listenfd(argv[1]);
    
    while (1) {
        if ((connfd = Accept(listenfd, (SA *) &clientaddr, &clientaddr_len)) == -1)
            continue;
        Getnameinfo((SA *) &clientaddr, clientaddr_len, hostname, MAXLINE, 
                    port, MAXLINE, 0);
        printf("Accepted connection from (%s, %s)\n", hostname, port);
        serve_client((int *)connfd);
        printf("Close connection from (%s, %s)\n", hostname, port);
    }

    // printf("%s", user_agent_hdr);
    return 0;
}

void *serve_client(void *connfd) {
    int fd = (int)connfd;
    rio_t rp;
    char msg[MAXLINE];

    Rio_readinitb(&rp, fd);
    
    if (parse_request(&rp, msg) < 0)
        clienterror(rp.rio_fd, "", "400", "Bad Request", "Bad request");
    else {
        response(&rp, msg);
    }
        
    close(fd);
}

int parse_request(rio_t *rp, char *req_buf) {
    char buf[MAXLINE];
    char method[16], url[MAXLINE], version[32];
    char host[MAXLINE] = {0};
    int add_host = 1;

    Rio_readlineb(rp, buf, MAXLINE);
    sscanf(buf, "%s %s %s", method, url, version);
    if (strcmp(method, "GET") != 0) 
        return -1;
    
    char *prefix = "http://";
    size_t prefix_len = strlen(prefix);
    char *ptr, *end;
    if ((ptr = strstr(url, prefix)) != NULL)
        ptr += prefix_len;
    else
        ptr = url;
    if ((end = strchr(ptr, '/')) == NULL)
        return -1;
    if (end != ptr) {
        *end = '\0';
        sprintf(host, "Host: %s\r\n", ptr);
        printf("%s", host);
        *end = '/';
    }
    if (*(end + 1) == 0)
        sprintf(url, "/index.html");
    else
        strcpy(url, end);
    
    sprintf(req_buf, "%s %s %s\r\n", method, url, "HTTP/1.0");

    do {
        Rio_readlineb(rp, buf, MAXLINE);
        if (strstr(buf, "Host:") != NULL) 
            add_host = 0;
        if (!strcmp(buf, "\r\n") && add_host) {
            if (strlen(host) == 0) 
                return -1;
            strcat(req_buf, host); 
        }
            
        strcat(req_buf, buf);

    } while (strcmp(buf, "\r\n"));

    return 0;
}

void response(rio_t *rp, char *msg) {
    char buf[MAXLINE], body[MAXLINE];
    
    /* Print the HTTP response body */
    sprintf(body, "<html><title>Hello World</title>");
    strcat(body, "<body bgcolor=""ffffff"">\r\n");
    sprintf(body, "<p>%s</p>\r\n", msg);
    strcat(body, "<hr><em>The Tiny Web server</em></body></html>\r\n");
 
    /* Print the HTTP response headers */
    // sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    Rio_writen(rp->rio_fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    Rio_writen(rp->rio_fd, buf, strlen(buf));
    sprintf(buf, "Content-Length: %ld\r\n", strlen(body));
    Rio_writen(rp->rio_fd, buf, strlen(buf));
    sprintf(buf, "Connection: close\r\n\r\n");
    Rio_writen(rp->rio_fd, buf, strlen(buf));
    Rio_writen(rp->rio_fd, body, strlen(body));
    close(rp);
}

/* $begin clienterror */
void clienterror(int fd, char *cause, char *errnum, 
		 char *shortmsg, char *longmsg) 
{
    char buf[MAXLINE];

    /* Print the HTTP response headers */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n\r\n");
    Rio_writen(fd, buf, strlen(buf));

    /* Print the HTTP response body */
    sprintf(buf, "<html><title>Tiny Error</title>");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "<body bgcolor=""ffffff"">\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "%s: %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "<p>%s: %s\r\n", longmsg, cause);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "<hr><em>The Tiny Web server</em>\r\n");
    Rio_writen(fd, buf, strlen(buf));
}
/* $end clienterror */