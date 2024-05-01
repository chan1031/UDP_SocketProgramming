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
#define MAX_CLIENTS 10
#define MAX_MESSAGE_SIZE 1024

typedef struct {
	struct sockaddr_in address;
	int socket;
	int id;
} Client;
Client clients[MAX_CLIENTS];
pthread_t threads[MAX_CLIENTS];
int clientCount = 0;
pthread_mutex_t mutex;

void *handleClient(void *arg) {
	int id = *((int *)arg);
	char buffer[MAX_MESSAGE_SIZE];
	ssize_t bytesRead;

	while (1) {
        char buf[BUF_SIZE];
        ssize_t bytes_read;
        // sending viedo file
        printf("sending video file...\n");
           size_t fsize;

        //video file
        FILE *file;
        char *filename = "video.mp4";
         // open video file
        file = fopen(filename, "rb");
        if (file == NULL) {
            perror("File opening failed");
            exit(EXIT_FAILURE);
        }

for (;;)   // loop for ever
  {
    char buffer[BUF_SIZE];
    size_t bytesread = fread(buffer, 1, sizeof buffer, file);

    // bytesread contains the number of bytes actually read
    if (bytesread == 0)
    {   
        char fileFinMessage[] = "fin";
        sendto(clients[id].socket, fileFinMessage, sizeof(fileFinMessage),0, (struct sockaddr*)&clients[id].address, sizeof(clients[id].address));
      // no bytes read => end of file
      break;
    }

    // send the data
    sendto(clients[id].socket, buffer, BUF_SIZE,0, (struct sockaddr*)&clients[id].address, sizeof(clients[id].address));
  
  } 
printf("send video compelte");
		
}
}

int main(){
    int serv_sock;
    struct sockaddr_in serv_adr;
    char message[BUF_SIZE];
    char buf[BUF_SIZE];
    socklen_t clnt_adr_sz = sizeof(struct sockaddr_in);
    ssize_t str_len;

    // Create socket
	if ((serv_sock = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
		perror("Socket creation failed");
		exit(EXIT_FAILURE);
	}

    //socket address
    memset(&serv_adr, 0, sizeof(serv_adr));
    serv_adr.sin_family=AF_INET;
    serv_adr.sin_addr.s_addr=INADDR_ANY;
    serv_adr.sin_port=htons(PORT);

    //binding socket
    if(bind(serv_sock, (struct sockaddr*)&serv_adr, sizeof(serv_adr)) == -1){
        perror("bind() error");
        exit(1);
    }

    //mutex Init
    pthread_mutex_init(&mutex, NULL);

    printf("waiting for client...\n");

    //Create a thraed for each client
    while(1){

        //receive message from client
        str_len=recvfrom(serv_sock, message, BUF_SIZE, 0, (struct sockaddr *)&clients[clientCount].address, &clnt_adr_sz);
         if (str_len < 0) {
            perror("recvfrom error");
            exit(1);
        }  

        //print message
        message[str_len] = '\0';
        printf("client %d say: %s\n", clientCount+1, message);

        //Add client to the list
        clients[clientCount].socket = serv_sock;
        clients[clientCount].id = clientCount;
        //create thread for client
        pthread_create(&threads[clientCount], NULL, handleClient, (void *)&clients[clientCount].id);
        clientCount++;
        //send a message to the client
        sprintf(message,"hello i am server, you are Client %d\n", clientCount);
        if (sendto(clients[clientCount-1].socket, message, strlen(message), 0, (struct sockaddr*)&clients[clientCount-1].address, sizeof(clients[clientCount - 1].address))< 0) {
            perror("sendto error");
            exit(1);
        }

    }
    close(serv_sock);
    pthread_mutex_destroy(&mutex);
    return 0;
    
}