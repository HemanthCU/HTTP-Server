/* 
 * tcpechosrv.c - A concurrent TCP echo server using threads
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>      /* for fgets */
#include <strings.h>     /* for bzero, bcopy */
#include <unistd.h>      /* for read, write */
#include <sys/socket.h>  /* for socket use */
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#define MAXLINE  8192  /* max text line length */
#define MAXBUF   8192  /* max I/O buffer size */
#define LISTENQ  1024  /* second argument to listen() */
#define MAXREAD  20000

int open_listenfd(int port);
void echo(int connfd);
void *thread(void *vargp);
char* getContentType(char *tgtpath);

int main(int argc, char **argv) {
    int listenfd, *connfdp, port, clientlen=sizeof(struct sockaddr_in);
    struct sockaddr_in clientaddr;
    pthread_t tid; 

    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(0);
    }
    port = atoi(argv[1]);

    listenfd = open_listenfd(port);
    while (1) {
        connfdp = malloc(sizeof(int));
        *connfdp = accept(listenfd, (struct sockaddr*)&clientaddr, &clientlen);
        if (*connfdp >= 0)
            pthread_create(&tid, NULL, thread, connfdp);
    }
}

char* getContentType(char *tgtpath) {
    char *temp1 = (char*) malloc (100*sizeof(char));
    char *temp2 = (char*) malloc (100*sizeof(char));
    char *temp3 = NULL;
    char *temp4 = (char*) malloc (100*sizeof(char));
    strcpy(temp1, tgtpath);
    temp3 = strrchr(temp1, '.');
    if (temp3 == NULL) {
        printf("ERROR in file type\n");
        return NULL;
    }
    temp2 = temp3 + 1;
    if (strcmp(temp2, "html") == 0) {
        strcpy(temp4, "text/html");
    } else if (strcmp(temp2, "txt") == 0) {
        strcpy(temp4, "text/plain");
    } else if (strcmp(temp2, "png") == 0) {
        strcpy(temp4, "image/png");
    } else if (strcmp(temp2, "gif") == 0) {
        strcpy(temp4, "image/gif");
    } else if (strcmp(temp2, "jpg") == 0) {
        strcpy(temp4, "image/jpg");
    } else if (strcmp(temp2, "ico") == 0) {
        strcpy(temp4, "image/x-icon");
    } else if (strcmp(temp2, "css") == 0) {
        strcpy(temp4, "text/css");
    } else if (strcmp(temp2, "js") == 0) {
        strcpy(temp4, "application/javascript");
    } else {
        temp4 = NULL;
    }

    return temp4;
}

/* thread routine */
void * thread(void * vargp) {
    int connfd = *((int *)vargp);
    pthread_detach(pthread_self());
    free(vargp);
    // Process the header to get details of request
    size_t n;
    int keepalive = 0;
    int first = 1;
    int msgsz;
    char buf[MAXLINE];
    char *resp = (char*) malloc (MAXREAD*sizeof(char));
    char msg[MAXREAD + 1];
    char *context = NULL;
    char *comd;
    char *host;
    char *temp = NULL;
    char *tgtpath;
    char *tgtpath1 = (char*) malloc (100*sizeof(char));
    char *httpver;
    char *contType;
    char c;
    FILE *fp;
    while(keepalive || first--) {
        printf("Waiting for data\n");
        n = read(connfd, buf, MAXLINE);
        printf("%d\n", (int)n);
        if ((int)n >= 0) {
            comd = strtok_r(buf, " \t\r\n\v\f", &context);
            tgtpath = strtok_r(NULL, " \t\r\n\v\f", &context);
            if (tgtpath[strlen(tgtpath) - 1] == '/')
                sprintf(tgtpath1, "./www%sindex.html", tgtpath);
            else
                sprintf(tgtpath1, "./www%s", tgtpath);
            httpver = strtok_r(NULL, " \t\r\n\v\f", &context);
            host = strtok_r(NULL, " \t\r\n\v\f", &context);
            host = strtok_r(NULL, " \t\r\n\v\f", &context);

            if (strcmp(httpver, "HTTP/1.1") == 0) {
                c = context[1];
                if (c != '\r') {
                    temp = strtok_r(NULL, " \t\r\n\v\f", &context);
                    temp = strtok_r(NULL, " \t\r\n\v\f", &context);
                    if (strcmp(temp, "Keep-alive") == 0) {
                        keepalive = 1;
                    } else {
                        keepalive = 0;
                    }
                } else {
                    keepalive = 0;
                }
            } else {
                keepalive = 0;
            }
            contType = getContentType(tgtpath1);
            printf("comd=%s tgtpath=%s httpver=%s host=%s keepalive=%d \n", comd, tgtpath1, httpver, host, keepalive);
            // Choose what to perform based on comd
            if (contType == NULL) {
                // TODO: Need to return webpage with error
                printf("ERROR in requested data type\n");
            } else if (strcmp(comd, "GET") == 0) {
                fp = fopen(tgtpath1, "r");
                fseek(fp, 0, SEEK_SET);
                msgsz = fread(msg, MAXREAD, 1, fp);
                sprintf(resp, "%s 200 Document Follows\r\nContent-Type:%s\r\nContent-Length:%d\r\n\r\n%s", httpver, contType, (int)strlen(msg), msg);
                write(connfd, resp, strlen(resp));
                fclose(fp);
            } else if (strcmp(comd, "PUT") == 0) {

            } else if (strcmp(comd, "POST") == 0) {

            } else if (strcmp(comd, "HEAD") == 0) {
                fp = fopen(tgtpath1, "r");
                fseek(fp, 0, SEEK_SET);
                msgsz = fread(msg, MAXREAD, 1, fp);
                sprintf(resp, "%s 200 Document Follows\r\nContent-Type:%s\r\nContent-Length:%d\r\n\r\n", httpver, contType, (int)strlen(msg));
                write(connfd, resp, strlen(resp));
                fclose(fp);
            } else {
                // TODO: Need to return webpage with error
            }
        } else {
            printf("No data received. Closing thread.\n");
            keepalive = 0;
        }
    }
    close(connfd);
    return NULL;
}

/* 
 * open_listenfd - open and return a listening socket on port
 * Returns -1 in case of failure 
 */
int open_listenfd(int port) {
    int listenfd, optval=1;
    struct sockaddr_in serveraddr;
  
    /* Create a socket descriptor */
    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        return -1;

    /* Eliminates "Address already in use" error from bind. */
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, 
                    (const void *)&optval , sizeof(int)) < 0)
        return -1;
    struct timeval tv;
    tv.tv_sec = 10;
    tv.tv_usec = 0;
    if (setsockopt(listenfd, SOL_SOCKET, SO_RCVTIMEO,
                    (struct timeval *)&tv,sizeof(struct timeval)) < 0)
        return -1;

    /* listenfd will be an endpoint for all requests to port
       on any IP address for this host */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET; 
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY); 
    serveraddr.sin_port = htons((unsigned short)port); 
    if (bind(listenfd, (struct sockaddr*)&serveraddr, sizeof(serveraddr)) < 0)
        return -1;

    /* Make it a listening socket ready to accept connection requests */
    if (listen(listenfd, LISTENQ) < 0)
        return -1;
    return listenfd;
} /* end open_listenfd */

