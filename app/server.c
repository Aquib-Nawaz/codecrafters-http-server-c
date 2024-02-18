#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/select.h>
#include "fcntl.h"
#include <pthread.h>
#include <sys/stat.h>

char *directory=NULL;
struct request_struct{
    char * method;
    char * path;
    char * userAgent;
    char * data;
};

void getPath(char* message, int len, struct request_struct* ret){
    int lastSpace = 0;
    for(int i=0; i<len; i++){
        if(message[i]==' '){
            if(lastSpace){
                ret->path = malloc(i-lastSpace);
                memcpy(ret->path, message+lastSpace+1, i-lastSpace-1);
                ret->path[i-lastSpace-1] = '\0';
                break;
            }
            else{
                lastSpace = i;
                ret->method = malloc(i+1);
                memcpy(ret->method, message, i);
                ret->method[i] = '\0';
            }
        }
    }
    char *lines = strtok(message, "\r\n");
    while(lines != NULL){
        if(strncmp(lines, "User-Agent:", 11)==0){
            int start=0;
            while(lines[start]!=' ')
                start++;
            start++;
            int end = start;
            while(lines[end]!='\r')
                end++;
            ret->userAgent = malloc(end-start);
            ret->userAgent[end-start-1]='\0';
            memcpy(ret->userAgent, lines+start, end-start-1);
        }
        else if(strncmp(lines-3, "\n\r\n",3)==0){
            ret->data = malloc(strlen(lines)+1);
            ret->data[strlen(lines)]='\0';
            strcpy(ret->data, lines);
        }
        lines = strtok(NULL, "\r\n");
    }
}

void read_request(int client_fd ){
//    int client_fd  = *(int *)arg;
    char read_buffer[1024];
    int msgLen = read(client_fd, read_buffer, 1023);
    if(msgLen==0){
        close(client_fd);
        return ;
    }
    read_buffer[msgLen] = '\0';
    struct request_struct ret;
    getPath(read_buffer, msgLen, &ret);

    printf("%s --- %s\n", ret.path, read_buffer);
    char write_buffer[1024] = "HTTP/1.1 404 Not Found\r\n\r\n";

    if(strcmp(ret.path, "/")==0){
        strcpy(write_buffer, "HTTP/1.1 200 OK\r\n\r\n");
        send(client_fd, write_buffer, strlen(write_buffer), 0);
        return;
    }
    else if(strlen(ret.path)>=6){
        char check_command[100];
        check_command[4] = '\0';
        memcpy(check_command, ret.path + 1 , 4);
        if(strcmp(check_command, "echo") == 0){
            int lastIdx = 6;
            while(lastIdx < strlen(ret.path) && ret.path[lastIdx]!=' ')
                lastIdx++;
            char random_string[lastIdx-6+1];
            memcpy(random_string, ret.path+6,lastIdx-6);
            random_string[lastIdx-6]='\0';
            snprintf(write_buffer,1023,"HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: %lu\r\n\r\n%s",
            strlen(random_string),random_string);
        }

        if(strcmp(check_command, "file")==0 && ret.path[5]=='s'){
            int lastIdx = 7;
            while(lastIdx < strlen(ret.path) && ret.path[lastIdx]!=' ')
                lastIdx++;
            if(directory!=NULL) {
                char file_name[lastIdx - 7 + 1 + strlen(directory)];
                strcpy(file_name, directory);
                memcpy(file_name+ strlen(directory),ret.path+7,lastIdx-7);
//                file_name[strlen(directory)]='/';
                file_name[lastIdx-7+ strlen(directory)]='\0';
                printf("file:- %s\n", file_name);
                int fd = open(file_name, O_RDONLY);
                if (fd != -1) {
                    struct stat st;
                    if (stat(file_name, &st) == 0) {
                        const char *response_format =
                                "HTTP/1.1 200 OK\r\nContent-Type: "
                                "application/octet-stream\r\nContent-Length: %zu\r\n\r\n";
                        char response[1024];
                        sprintf(response, response_format, st.st_size);
                        send(client_fd, response, strlen(response), 0);
                        // Send the file contents
                        ssize_t bytes_read;
                        char buffer[1024];
                        while ((bytes_read = read(fd, buffer, 1024)) > 0) {
                            printf("%zd\n", bytes_read);
                            send(client_fd, buffer, bytes_read, 0);
                        }
                        close(fd);
                    }
                }
            }
//            return;
        }

        if(strlen(ret.path)==11)
            memcpy(check_command, ret.path+1, 10);
        check_command[10]='\0';
        if(strcmp(check_command, "user-agent") == 0) {
            snprintf(write_buffer,1023,"HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: %lu\r\n\r\n%s",
                     strlen(ret.userAgent),ret.userAgent);
        }

        if(strcmp(ret.method, "POST")==0){
            char file_name[100];
            char file_path[200];
            strcpy(file_name, ret.path+7);
            if(directory!=NULL){
                snprintf(file_path, 200, "%s%s", directory, file_name);
                FILE *fptr = fopen(file_name, "w");
                fprintf(fptr, "%s", ret.data);
                fclose(fptr);
            }
        }

    }
    send(client_fd, write_buffer, strlen(write_buffer), 0);
    close(client_fd);
}

int main(int argc, char*argv[]) {
	// Disable output buffering
	setbuf(stdout, NULL);
    if(argc==3){
        directory = argv[2];
    }
	// You can use print statements as follows for debugging, they'll be visible when running tests.
	printf("Logs from your program will appear here!\n");

//	 Uncomment this block to pass the first stage

	 int server_fd, client_addr_len;
	 struct sockaddr_in client_addr;

	 server_fd = socket(AF_INET, SOCK_STREAM, 0);
	 if (server_fd == -1) {
	 	printf("Socket creation failed: %s...\n", strerror(errno));
	 	return 1;
	 }

	 // Since the tester restarts your program quite often, setting REUSE_PORT
	 // ensures that we don't run into 'Address already in use' errors
	 int reuse = 1;
	 if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse)) < 0) {
	 	printf("SO_REUSEPORT failed: %s \n", strerror(errno));
	 	return 1;
	 }

	 struct sockaddr_in serv_addr = { .sin_family = AF_INET ,
	 								 .sin_port = htons(4221),
	 								 .sin_addr = { htonl(INADDR_ANY) },
	 								};

	 if (bind(server_fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) != 0) {
	 	printf("Bind failed: %s \n", strerror(errno));
	 	return 1;
	 }

	 int connection_backlog = 5;
	 if (listen(server_fd, connection_backlog) != 0) {
	 	printf("Listen failed: %s \n", strerror(errno));
	 	return 1;
	 }

	 printf("Waiting for a client to connect...\n");
	 client_addr_len = sizeof(client_addr);

    fd_set master, current;
    FD_SET(server_fd, &master);
    int max_socket_fd = server_fd+1;
    int client_fd;

//     while(1) {
//         int client_fd = accept(server_fd, (struct sockaddr *) &client_addr, &client_addr_len);
//         printf("Client connected on socket %d\n", client_fd);
//         pthread_t thread;
//         if (pthread_create(&thread, NULL, read_request,
//                            (void *)&client_fd) != 0) {
//             printf("Failed to create thread\n");
//             close(client_fd);
//         } else {
//             pthread_detach(thread);
//         }
////         read_request((void*)&client_fd);
//     }
    for(;;){
        current = master;
        if(select(max_socket_fd,&current,NULL,NULL,NULL)==-1){
            perror("select");
        }
        for(int i=0; i<=max_socket_fd; i++){
            if(FD_ISSET(i, &current)){
                if(i==server_fd){
                    client_fd = accept(server_fd, (struct sockaddr *) &client_addr, &client_addr_len);
                    if(client_fd==-1){
                        perror("accept");
                    }
                    FD_SET(client_fd, &master);
                    if(client_fd>=max_socket_fd){
                        max_socket_fd = client_fd+1;
                    }
                    printf("Client connected at socket %d\n", client_fd);
                }
                else{
                    read_request(i);
                    FD_CLR(i, &master);
                }
            }
        }
    }


	 close(server_fd);

	return 0;
}
