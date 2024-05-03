#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/socket.h>
#include <pthread.h>
#include <unistd.h>

#define PORT 8888
#define BUF_SIZE 1254
#define MAX_CLIENTS 100

typedef struct
{
    struct sockaddr_in address;
    int socket;
    int id;
} Client;

Client clients[MAX_CLIENTS];
pthread_t threads[MAX_CLIENTS];
int clientCount = 0;
pthread_mutex_t mutex;

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

void *handleClient(void *arg)
{
    int id = *((int *)arg);
    ssize_t bytesRead;

    // video file
    FILE *file;
    char *filename = "image.jpg";
    // open video file
    file = fopen(filename, "rb");
    if (file == NULL)
    {
        perror("File opening failed");
        exit(EXIT_FAILURE);
    }
    char buf[BUF_SIZE];
    ssize_t bytes_read;
    // check file size
    fseek(file, 0, SEEK_END);
    size_t file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    // define loop count
    size_t loopcount = file_size / BUF_SIZE;
    // what is the residue
    size_t residue = file_size % BUF_SIZE;
    char buffer[BUF_SIZE];
    int i=0;
    pthread_mutex_lock(&mutex);
    strcpy(buffer,"retry");
    while (1)
    {   
        printf("received message: %s\n", buffer);
        if (!strcmp(buffer, "success"))
        {
            break;
        }
        else if(!strcmp(buffer, "retry"))
        {   printf("sending videofile\n");
            // data sending
            for (int i = 0; i < loopcount; i++)
            {

                size_t bytesread = fread(buffer, 1, BUF_SIZE, file);

                sendto(clients[id].socket, buffer, BUF_SIZE, 0, (struct sockaddr *)&clients[id].address, sizeof(clients[id].address));
            }

            // residue data sending
            size_t bytesread = fread(buffer, 1, residue, file);
            sendto(clients[id].socket, buffer, residue, 0, (struct sockaddr *)&clients[id].address, sizeof(clients[id].address));

            printf("sending video file completed\n");
            char fileFinMessage[] = "fin";

            if (sendto(clients[id].socket, fileFinMessage, sizeof(fileFinMessage), 0, (struct sockaddr *)&clients[id].address, sizeof(clients[id].address)) < 0)
            {
                perror("sendto() failed");
                exit(EXIT_FAILURE);
            }
            // sending checksum
            unsigned short checksum = calculateChecksum(file);
            sprintf(buffer, "%d", checksum);
            printf("checksum is %d \n", checksum);
            sendto(clients[id].socket, buffer, BUF_SIZE, 0, (struct sockaddr *)&clients[id].address, sizeof(clients[id].address));

        }
        //is data transfer has not error?
        bytes_read=recvfrom(clients[id].socket, buffer, sizeof(buffer), 0, NULL, NULL);
        buffer[bytes_read]='\0';
        
    }

    fclose(file);
    pthread_mutex_lock(&mutex);
    return 0;
}

int main()
{
    int serv_sock;
    struct sockaddr_in serv_adr;
    char message[BUF_SIZE];
    char buf[BUF_SIZE];
    socklen_t clnt_adr_sz = sizeof(struct sockaddr_in);
    ssize_t str_len;

    // Create socket
    if ((serv_sock = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // socket address
    memset(&serv_adr, 0, sizeof(serv_adr));
    serv_adr.sin_family = AF_INET;
    serv_adr.sin_addr.s_addr = INADDR_ANY;
    serv_adr.sin_port = htons(PORT);

    // binding socket
    if (bind(serv_sock, (struct sockaddr *)&serv_adr, sizeof(serv_adr)) == -1)
    {
        perror("bind() error");
        exit(1);
    }

    // mutex Init
    pthread_mutex_init(&mutex, NULL);

    printf("waiting for client...\n");

    // Create a thraed for each client
    while (1)
    {
        // receive message from client
        str_len = recvfrom(serv_sock, message, BUF_SIZE, 0, (struct sockaddr *)&clients[clientCount].address, &clnt_adr_sz);
        if (str_len < 0)
        {
            perror("recvfrom error");
            exit(1);
        }

        // print message
        message[str_len] = '\0';
        printf("client %d say: %s\n", clientCount + 1, message);

        // Add client to the list
        clients[clientCount].socket = serv_sock;
        clients[clientCount].id = clientCount;
        
        clientCount++;
        // send a message to the client
        sprintf(message, "hello i am server, you are Client %d\n", clientCount);
        if (sendto(clients[clientCount - 1].socket, message, strlen(message), 0, (struct sockaddr *)&clients[clientCount - 1].address, sizeof(clients[clientCount - 1].address)) < 0)
        {
            perror("sendto error");
            exit(1);
        }
        // create thread for client
        pthread_create(&threads[clientCount+1], NULL, handleClient, (void *)&clients[clientCount+1].id);
        pthread_join(threads[clientCount+1], NULL);
    }

    close(serv_sock);
    pthread_mutex_destroy(&mutex);
    return 0;
}