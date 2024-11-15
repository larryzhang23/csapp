#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "csapp.h"
#include "cache.h"


/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";

/* cache and total_object are protected by reader-writer lock */
/* proxy cache */
static Cache cache;

void *serve_client(void *connfd);

int parse_request(rio_t *rp, char *req_buf, char *host, char *port, char *url);

int proxy_request(int connfd, char *req_buf, char *host, char *port, char *url);

void debug_respond(int fd, char *msg);

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
    pthread_t tid;

    listenfd = Open_listenfd(argv[1]);

    /* init clock_mutex */
    cache_init(&cache);
    
    while (1) {
        if ((connfd = Accept(listenfd, (SA *) &clientaddr, &clientaddr_len)) == -1)
            continue;
        Getnameinfo((SA *) &clientaddr, clientaddr_len, hostname, MAXLINE, 
                    port, MAXLINE, 0);
        printf("Accepted connection from (%s, %s)\n", hostname, port);
        Pthread_create(&tid, NULL, serve_client, (void *)connfd);
        printf("sprawn thread: %lu\n", tid);
    }

    // printf("%s", user_agent_hdr);
    return 0;
}

void *serve_client(void *connfd) {
    Pthread_detach(Pthread_self());
    int fd = (int)connfd;
    rio_t rp;
    char req[MAXLINE];
    char host[MAXLINE], port[10], url[MAXLINE];

    Rio_readinitb(&rp, fd);
    
    if (parse_request(&rp, req, host, port, url) != 0)
        clienterror(rp.rio_fd, "parse request failed", "400", "Bad Request", "Bad request");
    
    printf("%s", req);
    if (proxy_request(fd, req, host, port, url) != 0)
        clienterror(rp.rio_fd, "proxy request failed", "400", "Bad Request", "Bad request");
    
    close(fd);
}

int parse_request(rio_t *rp, char *req_buf, char *host, char *port, char *full_url) {
    char buf[MAXLINE], url[MAXLINE];
    char method[16], version[32];
    int find_host = 0;
    int add_host = 1;

    Rio_readlineb(rp, buf, MAXLINE);
    sscanf(buf, "%s %s %s", method, url, version);
    strcpy(full_url, url);
    if (strcmp(method, "GET") != 0) 
        return -1;
    
    char *prefix = "http://";
    size_t prefix_len = strlen(prefix);
    char *ptr, *end;
    if ((ptr = strstr(url, prefix)) != NULL)
        ptr += prefix_len;
    else
        ptr = url;
    /* parse host name */
    if ((end = strchr(ptr, '/')) == NULL)
        return -1;
    if (end != ptr) {
        *end = '\0';
        /* parse port number if exists */
        char *tmp;
        if ((tmp = strchr(ptr, ':')) != NULL) {
            strcpy(port, tmp + 1);
            *tmp = 0;
            sprintf(host, "%s", ptr);
            *tmp = ':';
        } else {
            sprintf(host, "%s", ptr);
            strcpy(port, "80");
        }
        find_host = 1;
        // printf("%s", host);
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
        if ((strstr(buf, "Connection:") != NULL) || (strstr(buf, "Proxy-Connection") != NULL))
            continue;
        /* add additional headers when meet the last line */
        if (!strcmp(buf, "\r\n")) {
            if (add_host) {
                if (find_host == 0) 
                    return -1;
                sprintf(req_buf, "%sHost: %s\r\n", req_buf, host);
            }
            strcat(req_buf, "Connection: close\r\n");
            strcat(req_buf, "Proxy-Connection: close\r\n");
        }
        /* add headers */
        strcat(req_buf, buf);

    } while (strcmp(buf, "\r\n"));

    return 0;
}

int proxy_request(int connfd, char *req_buf, char *host, char *port, char *url) {
    rio_t rp;
    char buf[MAXLINE];
    char object[MAX_OBJECT_SIZE];
    char *obj_ptr = object;
    char *obj_ptr_end = object + MAX_OBJECT_SIZE;
    int save_to_cache = 1;
    ssize_t n;

    /* search cache first */
    if ((n = read_cache(&cache, url, object, MAX_OBJECT_SIZE)) > 0) {
        Rio_writen(connfd, object, n);
        return 0;
    }

    int clientfd;
    if ((clientfd = Open_clientfd(host, port)) < 0)
        return -1;
    
    Rio_readinitb(&rp, clientfd);

    /* write request to server */
    Rio_writen(clientfd, req_buf, strlen(req_buf));

    /* read header */
    if (Rio_readlineb(&rp, buf, MAXLINE) <= 0) {
        close(clientfd);
        return -1;
    }
    
    if (obj_ptr + strlen(buf) < obj_ptr_end) {
        strcpy(obj_ptr, buf);
        obj_ptr += strlen(buf);
    } else 
        save_to_cache = 0;
        
    Rio_writen(connfd, buf, strlen(buf));
    do {
        if (Rio_readlineb(&rp, buf, MAXLINE) <= 0) {
            close(clientfd);
            return -1;
        }

        if (obj_ptr + strlen(buf) < obj_ptr_end) {
            strcpy(obj_ptr, buf);
            obj_ptr += strlen(buf);
        } else 
            save_to_cache = 0;
        Rio_writen(connfd, buf, strlen(buf));
    } while (strcmp(buf, "\r\n"));


    /* read data */
    while ((n = Rio_readnb(&rp, buf, MAXLINE)) > 0) {
        // printf("read %ld bytes, buf:%s\n", n, buf);
        if (obj_ptr + n <= obj_ptr_end) {
            strncpy(obj_ptr, buf, n);
            obj_ptr += n;
        } else 
            save_to_cache = 0;
        Rio_writen(connfd, buf, n);
    }
    
    /* error handling */
    close(clientfd);
    if (n < 0)
        return -1;
    
    /* save to cache if possible */
    write_cache(&cache, url, object, obj_ptr - object);
    return 0;
}   

void debug_respond(int fd, char *msg) {
    char buf[MAXLINE], body[MAXLINE];
    
    /* Print the HTTP response body */
    sprintf(body, "<html><title>Hello World</title>");
    strcat(body, "<body bgcolor=""ffffff"">\r\n");
    sprintf(body, "<p>%s</p>\r\n", msg);
    strcat(body, "<hr><em>The Tiny Web server</em></body></html>\r\n");
 
    /* Print the HTTP response headers */
    // sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-Length: %ld\r\n", strlen(body));
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Connection: close\r\n\r\n");
    Rio_writen(fd, buf, strlen(buf));
    Rio_writen(fd, body, strlen(body));
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