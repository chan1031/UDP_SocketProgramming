#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/socket.h>

#define BUFSIZE 1254
#define PORT 8888

unsigned short calculateChecksum(FILE *file)
{
    fseek(file, 0, SEEK_SET);
    unsigned short checksum = 0;
    int ch;

    while ((ch = fgetc(file)) != EOF)
    {
        checksum += (unsigned char)ch; 
    }

    fseek(file, 0, SEEK_SET);
    return checksum;
}

void fileRecive(int sock, struct sockaddr_in *serv_addr, socklen_t adr_sz)
{
    FILE *file = fopen("received_image.jpg", "wb");
    if (file == NULL)
    {
        perror("File opening failed");
        exit(EXIT_FAILURE);
    }

    char message[BUFSIZE];
    size_t nbyte = BUFSIZE;
    while (1)
    {
        nbyte = recvfrom(sock, message, nbyte, 0, (struct sockaddr *)serv_addr, &adr_sz);
        if (!strcmp(message, "fin"))
        {
            printf("File received successfully.\n");
            break;
        }
        fwrite(message, nbyte, 1, file);
    }

    fclose(file);
}

int main()
{
    int sock;
    char message[BUFSIZE];
    int str_len;
    socklen_t adr_sz;

    struct sockaddr_in serv_addr, client_addr;

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == -1)
    {
        printf("socket() error\n");
        exit(1);
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    serv_addr.sin_port = htons(PORT);

    char hello_message[] = "hello i am client";
    sendto(sock, hello_message, strlen(hello_message), 0, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    adr_sz = sizeof(client_addr);
    str_len = recvfrom(sock, message, BUFSIZE, 0, (struct sockaddr *)&client_addr, &adr_sz);

    message[str_len] = '\0';
    printf("Server say: %s\n", message);

    fileRecive(sock, &serv_addr, adr_sz);

    FILE *file2;
    char *filename = "received_image.jpg";
    
    if (file2 == NULL)
    {
        perror("File opening failed");
        exit(EXIT_FAILURE);
    }
    // checksum 검사
    recvfrom(sock, message, BUFSIZE, 0, (struct sockaddr *)&client_addr, &adr_sz);
    while (1)
    {   
        file2 = fopen(filename, "rb");
        printf("received checksum = %s\n", message);
        unsigned short checksum = calculateChecksum(file2);
        printf("calcualte checksum = %d\n", checksum);
        char checksumCheck[BUFSIZE];
        sprintf(checksumCheck, "%d", checksum);
        if (!strcmp(message, checksumCheck))
        {   
            printf("checksum correct\n");
            sprintf(message, "success");
            printf("%s\n", message);
            message[7] = '\0';
            sendto(sock, message, strlen(message), 0, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
            fclose(file2);
            break;
        }
        else
        {
            printf("checksum incorrect");
            printf("\n");
            char retry_message[BUFSIZE] = "retry";
            retry_message[6] = '\0';
            sendto(sock, retry_message, strlen(retry_message), 0, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
            fileRecive(sock, &serv_addr, adr_sz);
        }
    }

    close(sock);

    return 0;
}