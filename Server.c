#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/socket.h>
#include <pthread.h>
#include <unistd.h>
//포트 번호와 수신받을 데이터의 사이즈 그리고 최대 받을 클라이언트의 숫자를 정의
#define PORT 8888
#define BUF_SIZE 1254
#define MAX_CLIENTS 100

//변수 선언
typedef struct
{
    struct sockaddr_in address;
    int socket;
    int id;
} Client; //struct는 멀티스레드를 통해 클라이언트가 부여받을 socket과 id를 할당하기 위해 따로 정의했습니다.
Client clients[MAX_CLIENTS]; //최대로 받을 클라이언트의 수
pthread_t threads[MAX_CLIENTS]; //최대로 활성화할 쓰레드의 수
int clientCount = 0;
pthread_mutex_t mutex; //mutex는 쓰레드를 구현하기 위한 자물쇠 같은 역할을 한다. 위애서 정의하고 밑에 쓰레드를 생성해준다.

/*
데이터 전송중 손실이 없었는지 체크하기 위한
체크섬 계산 함수
*/
unsigned short calculateChecksum(FILE *file)
{
    fseek(file, 0, SEEK_SET); //파일의 포인터를 처음으로
    unsigned short checksum = 0;
    int ch;
    while ((ch = fgetc(file)) != EOF)
    {
        checksum += (unsigned char)ch; //파일이 끝날떄까지 파일의 비트 값을 계속해서 더하여 체크섬을 구함
    }
    fseek(file, 0, SEEK_SET); //다시 파일의 포인터를 처음으로 돌림
    return checksum;
    // 이제 원본 파일의 체크섬 값을 구했고 클라이언트에게 전송한다. 그러면 클라이언트는 받은 파일의 체크섬 값을 구하고 비교한다.
}
//멀티 쓰레드 처리를 위한 client 핸들러
void *handleClient(void *arg)
{   
    //함수에서 쓸 변수들을 선언합니다.
    int id = *((int *)arg);
    ssize_t bytesRead;
    FILE *file;
    char *filename = "image.jpg"; //서버가 클라이언트에게 보낼 파일 이름은 image.jpg이다.
    file = fopen(filename, "rb");
    //파일의 포인터를 맨 앞으로 전송한다.
    fseek(file, 0, SEEK_SET);
    if (file == NULL)
    {
        perror("File opening failed");
        exit(EXIT_FAILURE);
    }

    ssize_t bytes_read;
    /*
        파일의 사이즈를 검사하고 데이터를 몇번 보낼지
        또한 1254바이트로 데이터를 보내기때문에 정확한 데이터를 보내려면
        원번파일과 /1254로 나눈 나머지 값을 따로 데이터로 전송해야하기에
        이를 구하는 과정.
    */
    fseek(file, 0, SEEK_END);
    size_t file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    size_t loopcount = file_size / BUF_SIZE;
    size_t residue = file_size % BUF_SIZE;
    char buffer[BUF_SIZE];
    int i = 0;
    strcpy(buffer, "retry");
    while (1)
    {
        printf("received message: %s\n", buffer);
        //클라이언트가 체크섬을 통해 데이터 전송에 문제가 없다고 하면 success를 보낸다.
        if (!strcmp(buffer, "success"))
        {
            printf("Client received file complete!!!\n");
            break;
        }
        //반면 문제가 생겼을 경우 retry를보내서 다시 데이터를 전송한다.
        else if (!strcmp(buffer, "retry"))
        {
            printf("sending file\n");
            // 앞서 정의한 대로 데이터를 전송한다.
            for (int i = 0; i < loopcount; i++)
            {
                size_t bytesread = fread(buffer, 1, BUF_SIZE, file);
                if (sendto(clients[id].socket, buffer, BUF_SIZE, 0, (struct sockaddr *)&clients[id].address, sizeof(clients[id].address)) == -1)
                {
                    perror("sendto() failed");
                    exit(EXIT_FAILURE);
                }
            }
            // 이제 residue 데이터를 클라이언트에 전송하고 데이터 전송을 마무리합니다.
            sleep(10); //sleep의 구현은 멀티쓰레드 확인을 위해서 임시로 넣어두었습니다.
            printf("Sleeping\n");
            //residue의 사이즈 만큼 데이터를 읽어와서 클라이언트에게 전송합니다.
            size_t bytesread = fread(buffer, 1, residue, file);
            sendto(clients[id].socket, buffer, residue, 0, (struct sockaddr *)&clients[id].address, sizeof(clients[id].address));
            printf("sending file completed\n");
            //데이터 전송이 완료되었음을 클라이언트에게 전송하는 과정
            char fileFinMessage[] = "fin";
            if (sendto(clients[id].socket, fileFinMessage, sizeof(fileFinMessage), 0, (struct sockaddr *)&clients[id].address, sizeof(clients[id].address)) < 0)
            {
                perror("sendto() failed");
                exit(EXIT_FAILURE);
            }
            // 체크섬을 클아이언트에게 전송
            unsigned short checksum = calculateChecksum(file);
            sprintf(buffer, "%d", checksum);
            printf("checksum is %d \n", checksum);
            sendto(clients[id].socket, buffer, BUF_SIZE, 0, (struct sockaddr *)&clients[id].address, sizeof(clients[id].address));
        }
        // 데이터가 문제가 있다면 buffer에 retry 문제가 없다면 success를 보냄
        bytes_read = recvfrom(clients[id].socket, buffer, sizeof(buffer), 0, NULL, NULL);
        buffer[bytes_read] = '\0';
    }
    fclose(file);
    // 해당되는 쓰레드의 클라이언트 소켓을 닫음
    close(clients[id].socket);

    return NULL;
}

int main()
{
    //메인 함수에서 쓰일 변수들을 지정합니다.
    int serv_sock;
    struct sockaddr_in serv_adr;
    char message[BUF_SIZE];
    char buf[BUF_SIZE];
    socklen_t clnt_adr_sz = sizeof(struct sockaddr_in);
    ssize_t str_len;
    // 소켓을 생성하는 함수입니다.
    // AF_INET은 IPv4를 의미
    // SOCK_DGRAM은 UDP를 의미
    //실패하면 -1을 반환하기에 처리를 해줍니다.
    if ((serv_sock = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    // 소켓 주소를 초기화하고 소켓에 주소를 할당합니다.
    memset(&serv_adr, 0, sizeof(serv_adr));
    serv_adr.sin_family = AF_INET;
    serv_adr.sin_addr.s_addr = INADDR_ANY;
    serv_adr.sin_port = htons(PORT);
    // 저장된 소켓 주소를 바인딩함수를 통해서 소켓에 등록시킵니다.
    if (bind(serv_sock, (struct sockaddr *)&serv_adr, sizeof(serv_adr)) == -1)
    {
        perror("bind() error");
        exit(1);
    }
    // 멀티쓰레드를 사용하기위해 mutex를 사용합니다.
    pthread_mutex_init(&mutex, NULL);
    // 각각의 클라이언트들에게 멀티쓰레드를 부여합니다.
    while (1)
    {
        printf("waiting for client...\n");
        // 클라이언트로부터 데이터를 받아옵니다.
        str_len = recvfrom(serv_sock, message, BUF_SIZE, 0, (struct sockaddr *)&clients[clientCount].address, &clnt_adr_sz);
        if (str_len < 0)
        {
            perror("recvfrom error");
            exit(1);
        }
        //받아온 데이터를 출력
        message[str_len] = '\0';
        printf("client %d say: %s\n", clientCount + 1, message);
        // 클라이어트의 소켓을 추가합니다. 이때 clientCount는 멀티스레드 구현을 위한 여러 클라이언트들의 소켓을 정의하기 위함입니다.
        clients[clientCount].socket = serv_sock;
        clients[clientCount].id = clientCount;
        //클라이언트 카운트 증가
        clientCount++;
        //클라이언트에게 hello 메시지를 보냅니다.
        sprintf(message, "hello i am server, you are Client %d\n", clientCount);
        if (sendto(clients[clientCount - 1].socket, message, strlen(message), 0, (struct sockaddr *)&clients[clientCount - 1].address, sizeof(clients[clientCount - 1].address)) < 0)
        {
            perror("sendto error");
            exit(1);
        }
        // 클라이언트에게 쓰레드를 할당합니다.
        pthread_create(&threads[clientCount - 1], NULL, handleClient, (void *)&clients[clientCount - 1].id);
    }
    // 쓰레드와 소켓 종료
    pthread_mutex_destroy(&mutex);
    close(serv_sock);

    return 0;
}
