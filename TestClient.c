#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/socket.h>

#define BUFSIZE 1254
#define PORT 8888
int main(){
    int sock;
    char message[BUFSIZE];
    int str_len;
    socklen_t adr_sz;
    int count=0;

    struct sockaddr_in serv_addr, client_addr;

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if(sock == -1){
        printf("socket() error\n");
        exit(1);
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    serv_addr.sin_port = htons(PORT);

    char hello_message[] = "hello i am client";
    sendto(sock, hello_message, strlen(hello_message), 0, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
    adr_sz = sizeof(client_addr);
    str_len=recvfrom(sock,message,BUFSIZE,0,(struct sockaddr*)&client_addr,&adr_sz);
   
    message[str_len] = '\0';
    printf("Server say: %s\n", message);

    //파일 전송     
    FILE *file = fopen("received_test.mp4", "wb");
    if (file == NULL) {
        perror("File opening failed");
        exit(EXIT_FAILURE);
    }

    int nbyte = BUFSIZE;
    while(1){
        
        nbyte = recvfrom(sock, message, BUFSIZE, 0, (struct sockaddr*)&serv_addr, &adr_sz);
        count++;
        if(!strcmp(message, "fin")){
            printf("File received successfully count is %d\n",count); 
            break;
        }
        fwrite(message, BUFSIZE, 1, file);
    }

    fclose(file);
    close(sock);
    
    return 0;
}