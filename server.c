#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <pthread.h>

#define MAXLINE 8192
#define SERVER_PORT 8888
#define LISTENNQ 5
#define MAXTHREAD 10


struct extensions{
    char *ext;
    char *ty;
}
 ext_type[] = {
    {"html", "text/html"},
    {"css", "text/css"},
    {"jpg", "image/jpeg"},
    {"pdf", "application/pdf"},   
    {"pptx", "application/vnd.openxmlformats-officedocument.presentationml.presentation"},
    {NULL, NULL}  
};


int threads_count = 0;


void* request_func(void *args);


int main(){
	    int listenfd, connfd;
        struct sockaddr_in servaddr, cliaddr;
        socklen_t len = sizeof(struct sockaddr_in);
        char ip_str[INET_ADDRSTRLEN] = {0};
        pthread_t threads[MAXTHREAD];
        

        if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
                printf("Error: init socket\n");
                return 0;
        }


        memset(&servaddr, 0, sizeof(servaddr));
        servaddr.sin_family = AF_INET;
        servaddr.sin_addr.s_addr = INADDR_ANY; 
        servaddr.sin_port = htons(SERVER_PORT); 

 
        if (bind(listenfd, (struct sockaddr *)&servaddr, sizeof(struct sockaddr)) < 0) {
                printf("Error: bind\n");
                return 0;
        }

        if (listen(listenfd, LISTENNQ) < 0) {
                printf("Error: listen\n");
                return 0;
        }

        while(1){
        	
        	if ((connfd = accept(listenfd, (struct sockaddr *)&cliaddr, &len))< 0) {
        		printf("Error: accept\n");
        		return 0;
        	}
        	inet_ntop(AF_INET, &(cliaddr.sin_addr), ip_str, INET_ADDRSTRLEN);
        	printf("Incoming connection from %s : %hu with fd: %d\n", ip_str, ntohs(cliaddr.sin_port), connfd);

        	if (pthread_create(&threads[threads_count], NULL, request_func, (void *)connfd) != 0) {
				printf("Error when creating thread %d\n", threads_count);
				return 0;
			}
			if (++threads_count >= MAXTHREAD) {
				break;
			}
		}
		printf("Max thread number reached, wait for all threads to finish and exit...\n");
		int i;
		for (i = 0; i < MAXTHREAD; ++i) {
			pthread_join(threads[i], NULL);
		}

        return 0;
    }





void* request_func(void *args){
	int connfd = (int)args;
	char rcv_buff[MAXLINE]={0};
	/*Get the request.*/
	int bytes_rcv = 0;
	while (1) {
		bytes_rcv += recv(connfd, rcv_buff + bytes_rcv, sizeof(rcv_buff) - bytes_rcv - 1, 0);
		
		if (bytes_rcv && rcv_buff[bytes_rcv - 1] == '\n')
			break;
	}
	
	/*Get the first line */
	char* pointer = rcv_buff;
	char* firstline = strsep(&pointer, "\r\n");
	//printf("%s\n", firstline);

	/*Check whether it's a http request && method is get.*/
	char* path;
	if((strstr(firstline, "HTTP/1.0")==NULL)&&(strstr(firstline, "HTTP/1.1")==NULL)){
		printf("Error: NOT HTTP Request.\n");
		close(connfd);
		threads_count--;
		printf("Close Connection.\n");
		return;
	}

	else if (strncmp(firstline, "GET ", 4) != 0){
		printf("Error: Not a GET method.\n");
		close(connfd);
		threads_count--;
		printf("Close Connection.\n");
		return;
	} else{
	/* If conditions met, get the path.*/
		strsep(&firstline, " ");
		path=strsep(&firstline, " ");
	    //printf("the path is %s\n", path);
	}

	/*Default index.html */
	if(strcmp(path, "/")==0){
		strcpy(path , "/index.html");
	}

	/*Get the type*/

	char* tmp= strrchr(path, '.');
	char* extion;
	if (tmp==NULL){
		extion="html";
	} else{
		extion=++tmp;
	}


	char* type=NULL;
	int j;
	for(j= 0; ext_type[j].ext != NULL; j++){
		if(strcmp(extion, ext_type[j].ext) == 0) {
			
			type=ext_type[j].ty;
			//printf("the type is %s\n", type);
			break;
		}
	}


	/*Constructing the response header.*/

	printf("Constructing the response.\n" );
	FILE *file;
	file = fopen(path+1, "r");
	char wrt_buff[MAXLINE] = {0};

    /* Unsupported type */
	if (type==NULL){
		printf("Unsupported extensions.\n");
		char* message="<!DOCTYPE html>\
			<html><head>\
			<title>415 Unsupported Media Type</title>\
			</head><body>\
			<h1>Unsupported Media Type</h1>\
			<p>The payload format is in an unsupported format.</p>\
			</body></html>";
		strcat(wrt_buff, "HTTP/1.1 415 Unsupported Media Type\r\n");
		strcat(wrt_buff, "Content-Type: text/html\r\n");
		strcat(wrt_buff, "Content-Length: ");
		strcat(wrt_buff, "198\r\n");
		strcat(wrt_buff, "Connection: Keep-Alive\r\n\r\n");
		strcat(wrt_buff, message);
		write(connfd, wrt_buff, strlen(wrt_buff));
	}
	/*File not found */
	else if (!file) {
		char* message="<!DOCTYPE html>\
			<html><head>\
			<title>404 Not Found</title>\
			</head><body>\
			<h1>Not Found</h1>\
			<p>The requested file is missing on this server.</p>\
			</body></html>";

		strcat(wrt_buff, "HTTP/1.1 404 Not Found\r\n");
		strcat(wrt_buff, "Content-Type: text/html\r\n");
		strcat(wrt_buff, "Content-Length: ");
		strcat(wrt_buff, "170\r\n");
		strcat(wrt_buff, "Connection: Keep-Alive\r\n\r\n");
		strcat(wrt_buff, message);
		write(connfd, wrt_buff, strlen(wrt_buff));


	} else {
		/*Using chunked transfer encoding. */
		strcat(wrt_buff, "HTTP/1.1 200 OK\r\n");
		strcat(wrt_buff, "Content-Type: ");
		strcat(wrt_buff, type);
		strcat(wrt_buff, "\r\n");

		/*Check coresponding gzip file. */ 
		char* gzippath=path+1;
		strcat(gzippath, ".gz");
		FILE *gzfile;
		gzfile = fopen(gzippath, "r");

		if(gzfile){
			fclose(file);
			file=gzfile;
			strcat(wrt_buff, "Content-Encoding: gzip\r\n");
			printf("Using gzip compression.\n" );
		}


		strcat(wrt_buff, "Transfer-Encoding: chunked\r\n\r\n");
		write(connfd, wrt_buff, strlen(wrt_buff));
		char* chunk[512]={0};
		int count;
		while (count=fread(chunk, 1, 512, file)) {
			strcpy(wrt_buff, "");
			sprintf(wrt_buff, "%x", count);
			strcat(wrt_buff, "\r\n");
			write(connfd, wrt_buff, strlen(wrt_buff));
			write(connfd, chunk, count);
			write(connfd, "\r\n", 2);
		}
		write(connfd, "0\r\n\r\n", 5);



	}
	close(connfd);
	threads_count--;
	printf("Close Connection.\n");
}










