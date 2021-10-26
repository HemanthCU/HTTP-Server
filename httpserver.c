/* 
 * httpserver.c - A server to handle different HTTP requests to load a website
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
#define MAXREAD  80000

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
        // Create a new thread only if a new connection arrives
        if (*connfdp >= 0)
            pthread_create(&tid, NULL, thread, connfdp);
    }
}

/* 
 * getContentType - gets the content type that needs to be added to HTTP response
 * Returns NULL in case of failure 
 */
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
    int keepalive = 0; /* Denotes if the current request is persistent */
    int first = 1; /* Denotes if this is the first execution of the while loop */
    int msgsz; /* Size of data read from the file */
    char buf[MAXLINE]; /* Request full message */
    char *resp = (char*) malloc (MAXBUF*sizeof(char)); /* Response header */
    unsigned char *msg = (char*) malloc (MAXREAD*sizeof(char)); /* Data read from the file */
    char *context = NULL; /* Pointer used for string tokenizer */
    char *comd; /* Incoming HTTP command */
    char *host; /* Incoming HTTP host */
    char *temp = NULL; /* Temporary pointer to check if connection is persistent */
    char *tgtpath; /* Incoming HTTP URL */
    char *tgtpath1 = (char*) malloc (100*sizeof(char)); /* Path to the file referenced by URL */
    char *httpver; /* Incoming HTTP version */
    char *contType; /* Content type of the data being returned */
    char *postdata; /* Postdata to be appended to the request */
    char c;
    FILE *fp; /* File descriptor to open the file */
    while (keepalive || first) {
        if (!first)
            printf("Waiting for incoming request\n");
        else
            first = 0;
        n = read(connfd, buf, MAXLINE);
        if ((int)n >= 0) {
            printf("Request received\n");
            comd = strtok_r(buf, " \t\r\n\v\f", &context);
            tgtpath = strtok_r(NULL, " \t\r\n\v\f", &context);
            // Default Page is handled here by looking for an index.html file at the given directory.
            // If it isn't found, the code will lead to 404 Not found error later.
            if (tgtpath[strlen(tgtpath) - 1] == '/')
                sprintf(tgtpath1, "./www%sindex.html", tgtpath);
            else
                sprintf(tgtpath1, "./www%s", tgtpath);
            httpver = strtok_r(NULL, " \t\r\n\v\f", &context);
            host = strtok_r(NULL, " \t\r\n\v\f", &context);
            host = strtok_r(NULL, " \t\r\n\v\f", &context);

            // Based on the incoming header data, decide if the connection must be persistent or not.
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
                    keepalive = 1;
                }
            }
            contType = getContentType(tgtpath1);
            //printf("comd=%s tgtpath=%s httpver=%s host=%s keepalive=%d \n", comd, tgtpath1, httpver, host, keepalive);
            // Choose what to perform based on comd
            if (contType == NULL || (strcmp(comd, "POST") == 0 && strcmp(contType, "text/html") != 0)) {
                // Process bad requests from the client
                // 1) Request to a file type that isn't supported
                // 2) Request to non-html files in POST requests
                printf("ERROR in requested data type\n");
                sprintf(msg, "<html><head><title>400 Bad Request</title></head><body><h2>400 Bad Request</h2></body></html>");
                sprintf(resp, "%s 400 Bad Request\r\nContent-Type:text/html\r\nContent-Length:%d\r\n\r\n%s", httpver, (int)strlen(msg), msg);
                write(connfd, resp, strlen(resp));
            } else if (strcmp(comd, "GET") == 0) {
                // Handle GET command - Read data from the tgtpath1 and respond with header data and the document
                if (contType[0] != 't')
                    fp = fopen(tgtpath1, "rb");
                else
                    fp = fopen(tgtpath1, "r");
                if (fp != NULL) {
                    fseek(fp, 0, SEEK_SET);
                    msgsz = fread(msg, 1, MAXREAD, fp);
                    sprintf(resp, "%s 200 Document Follows\r\nContent-Type:%s\r\nContent-Length:%d\r\n\r\n", httpver, contType, msgsz);
                    write(connfd, resp, strlen(resp));
                    write(connfd, msg, msgsz);
                    fclose(fp);
                } else {
                    // Process requests to files that don't exist
                    sprintf(msg, "<html><head><title>404 File Not Found</title></head><body><h2>404 File Not Found</h2></body></html>");
                    sprintf(resp, "%s 404 File Not Found\r\nContent-Type:text/html\r\nContent-Length:%d\r\n\r\n%s", httpver, (int)strlen(msg), msg);
                    write(connfd, resp, strlen(resp));
                }
            } else if (strcmp(comd, "POST") == 0) {
                // Handle POST command - Read data from the tgtpath1 and respond with header data
                // and the document appended with the postdata sent in the request
                postdata = strtok_r(NULL, " \t\r\n\v\f", &context);
                if (contType[0] != 't')
                    fp = fopen(tgtpath1, "rb");
                else
                    fp = fopen(tgtpath1, "r");
                if (fp != NULL) {
                    fseek(fp, 0, SEEK_SET);
                    msgsz = fread(msg, 1, MAXREAD, fp);
                    sprintf(resp, "%s 200 Document Follows\r\nContent-Type:%s\r\nContent-Length:%d\r\n\r\n<html><body><pre><h1>%s</h1></pre>",
                            httpver, contType, msgsz, postdata);
                    write(connfd, resp, strlen(resp));
                    write(connfd, msg, msgsz);
                    fclose(fp);
                } else {
                    // Process requests to files that don't exist
                    sprintf(msg, "<html><head><title>404 File Not Found</title></head><body><h2>404 File Not Found</h2></body></html>");
                    sprintf(resp, "%s 404 File Not Found\r\nContent-Type:text/html\r\nContent-Length:%d\r\n\r\n%s", httpver, (int)strlen(msg), msg);
                    write(connfd, resp, strlen(resp));
                }
            } else if (strcmp(comd, "HEAD") == 0) {
                // Handle HEAD command - Read data from the tgtpath1 and respond with header data only
                if (contType[0] != 't')
                    fp = fopen(tgtpath1, "rb");
                else
                    fp = fopen(tgtpath1, "r");
                if (fp != NULL) {
                    fseek(fp, 0, SEEK_SET);
                    msgsz = fread(msg, 1, MAXREAD, fp);
                    sprintf(resp, "%s 200 Document Follows\r\nContent-Type:%s\r\nContent-Length:%d\r\n\r\n", httpver, contType, msgsz);
                    write(connfd, resp, strlen(resp));
                    fclose(fp);
                } else {
                    // Process requests to files that don't exist
                    sprintf(msg, "<html><head><title>404 File Not Found</title></head><body><h2>404 File Not Found</h2></body></html>");
                    sprintf(resp, "%s 404 File Not Found\r\nContent-Type:text/html\r\nContent-Length:%d\r\n\r\n%s", httpver, (int)strlen(msg), msg);
                    write(connfd, resp, strlen(resp));
                }
            } else {
                // Process Internal Server errors like HTTP commands that are not supported
                sprintf(msg, "<html><head><title>500 Internal Server Error</title></head><body><h2>500 Internal Server Error</h2></body></html>");
                sprintf(resp, "%s 500 Internal Server Error\r\nContent-Type:text/html\r\nContent-Length:%d\r\n\r\n%s", httpver, (int)strlen(msg), msg);
                write(connfd, resp, strlen(resp));
            }
        } else {
            printf("No data received.\n");
            keepalive = 0;
        }
    }
    printf("Closing thread.\n");
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

    /* Sets a timeout of 10 secs. */
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

