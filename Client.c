#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/socket.h>

#define BUFSIZE 1254
#define PORT 8888
//서버에서 정의한것과 동일한 체크섬 계산 함수입니다. 서버와 동일한 함수입니다.
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
//서버로부터 파일을 받는 함수
void fileRecive(int sock, struct sockaddr_in *serv_addr, socklen_t adr_sz)
{   
    //파일을 연다.
    FILE *file = fopen("./received_image.jpg", "wb"); //받아오는 파일의 값은 received image.jpg로 정의하고 현재 파일에다가 저장시킨다.
    //파일 값이 널일 경우 오류처리
    if (file == NULL)
    {   
        perror("File opening failed");
        exit(EXIT_FAILURE);
    }

    char message[BUFSIZE];
    size_t nbyte = BUFSIZE;
    // 서버로부터 받은 데이터를 파일에 저장하기 위한 반복문
    while (1)
    {   //서버로부터 데이터를 할당받는다. 이때 nbyte에는 할당받은 데이터의 크기가 들어간다.
        nbyte = recvfrom(sock, message, nbyte, 0, (struct sockaddr *)serv_addr, &adr_sz);
        //서버로부터 fin을 받으면 while을 빠져나간다.
        if (!strcmp(message, "fin"))
        {
            printf("File received successfully.\n");
            break;
        }
        //받은 데이터들을 파일에 작성한다.
        fwrite(message, nbyte, 1, file);
    }

    fclose(file);
}

int main()
{   //메인함수 구현을 위한 변수 정의
    int sock;
    char message[BUFSIZE];
    int str_len;
    socklen_t adr_sz;
    struct sockaddr_in serv_addr, client_addr;
    //소켓을 정의한다. SOCK_DGAM을 통해서 UDP통신을 한다.
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == -1)
    {
        printf("socket() error\n");
        exit(1);
    }
    //정보 초기화 및 주소 정보 등록
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr("3.35.218.215"); //아마존 리눅스 서버에 접속하기 위한 IP주소
    serv_addr.sin_port = htons(PORT); //htons는 네트워크 통신과정에서 생기는 빅엔디안,리틀엔디안 이슈해결을 위해 빅엔디안 방식으로 바꾸는 함수입니다.
    //서버에게 hello 메시지를 보내는 과정
    char hello_message[] = "hello i am client";
    sendto(sock, hello_message, strlen(hello_message), 0, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    adr_sz = sizeof(client_addr);
    //서버로부터 메시지를 받는다.
    str_len = recvfrom(sock, message, BUFSIZE, 0, (struct sockaddr *)&client_addr, &adr_sz);
    // terminate string를 작성해야 서버에서 값을 받을때 정확한 값을 받을수 있다. 정의하지 않으면 원하는 값이 나오지 않을수 있기에 \0을 작성해준다.
    message[str_len] = '\0';
    printf("Server say: %s\n", message);
    //파일을 받아오는 함수
    fileRecive(sock, &serv_addr, adr_sz);
    //비트섬 검사를 위해 파일을 정의한다.
    FILE *file2;
    char *filename = "./received_image.jpg";
    //파일 오류처리
    if (file2 == NULL)
    {   
        perror("File opening failed");
        exit(EXIT_FAILURE);
    }
    // 서버로부터 원본 파일의 체크섬 값을 가져옵니다.
    recvfrom(sock, message, BUFSIZE, 0, (struct sockaddr *)&client_addr, &adr_sz);
    //이제 서버로부터 받은 비트섬값과 내가 받은 파일의 비트섬 값을 비교하여 다르다면 retry를 일치하면 success를 서버에게 보내는 기능을 구현한다.
    while (1)
    {   
        file2 = fopen(filename, "rb");
        printf("received checksum = %s\n", message);
        unsigned short checksum = calculateChecksum(file2);
        printf("calcualte checksum = %d\n", checksum);
        char checksumCheck[BUFSIZE];
        sprintf(checksumCheck, "%d", checksum);
        //체크섬이 일치하는지 여부를 검사합니다.
        if (!strcmp(message, checksumCheck))
        {   //체크섬이 일치하는 경우
            printf("checksum correct\n");
            sprintf(message, "success");
            printf("%s\n", message);
            sendto(sock, message, strlen(message), 0, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
            fclose(file2);
            break;
        }
        //체크섬이 일치하지 않기에 서버에게 파일을 다시 요청한다.
        else
        {
            printf("checksum incorrect");
            printf("\n");
            char retry_message[BUFSIZE] = "retry";
            sendto(sock, retry_message, strlen(retry_message), 0, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
            //파일 수신함수를 호출한다.
            fileRecive(sock, &serv_addr, adr_sz);
        }
    }
    //소켓을 닫고 종료한다.
    close(sock);

    return 0;
}