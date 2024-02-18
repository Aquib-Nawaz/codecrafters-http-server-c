#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

struct request_struct{
    char * method;
    char * path;
};

void getPath(char* message, int len, struct request_struct* ret){
    int lastSpace = 0;
    for(int i=0; i<len; i++){
        if(message[i]==' '){
            if(lastSpace){
                ret->path = malloc(i-lastSpace);
                memcpy(ret->path, message+lastSpace+1, i-lastSpace-1);
                ret->path[i-lastSpace-1] = '\0';
                return;
            }
            else{
                lastSpace = i;
                ret->method = malloc(i+1);
                memcpy(ret->method, message, i);
                ret->method[i] = '\0';
            }
        }
    }
}

void read_request(int client_fd){
    char read_buffer[1024];
    int msgLen = read(client_fd, read_buffer, 1023);
    read_buffer[msgLen] = '\0';
    struct request_struct ret;
    getPath(read_buffer, msgLen, &ret);
    printf("%s --- %s\n", ret.path, read_buffer);
    char write_buffer[1024] = "HTTP/1.1 404 Not Found\r\n\r\n";

    if(strcmp(ret.path, "/")==0){
        strcpy(write_buffer, "HTTP/1.1 200 OK\r\n\r\n");
        send(client_fd, write_buffer, strlen(write_buffer), 0);

    }
    else if(strlen(ret.path)>=6){
        char check_echo[5];
        check_echo[4] = '\0';
        memcpy(check_echo,ret.path+1 ,4);
        if(strcmp(check_echo, "echo")==0){
            int lastIdx = 6;
            while(lastIdx < strlen(ret.path) && ret.path[lastIdx]!=' ')
                lastIdx++;
            char random_string[lastIdx-6+1];
            memcpy(random_string, ret.path+6,lastIdx-6);
            random_string[lastIdx-6]='\0';
            snprintf(write_buffer,1023,"HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: %lu\r\n\r\n%s",
            strlen(random_string),random_string);
        }
    }
    send(client_fd, write_buffer, strlen(write_buffer), 0);
    close(client_fd);
}

int main() {
	// Disable output buffering
	setbuf(stdout, NULL);

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

	 int client_fd = accept(server_fd, (struct sockaddr *) &client_addr, &client_addr_len);
     read_request(client_fd);
	 printf("Client connected\n");



	 close(server_fd);

	return 0;
}
