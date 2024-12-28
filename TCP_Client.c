#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define NAME_SIZE 50

char name[NAME_SIZE];

void *receive_messages(void *sock) {
    int socketfd = *(int *)sock;
    char buffer[BUFFER_SIZE];
    int bytes_read;

    while ((bytes_read = read(socketfd, buffer, sizeof(buffer))) > 0) {
        buffer[bytes_read] = '\0';
        printf("%s\n", buffer);

		char *ptr = strtok(buffer, ":");
		while(ptr != NULL){
			if(strncmp(ptr, "exit", 4) == 0){
				printf("\n클라이언트가 퇴장했습니다...\n");
    			break;
			}
			ptr = strtok(NULL, " ");
		}
		
	}
    return NULL;
}

int main() {
    int sock;
    struct sockaddr_in server_addr;
    char message[BUFFER_SIZE];
    pthread_t tid;

    // 소켓 생성
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("소캣 생성 실패");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);

    // 서버 주소 설정
    if (inet_pton(AF_INET, "10.200.4.180", &server_addr.sin_addr) <= 0) {
        perror("서버 주소가 없음");
        exit(EXIT_FAILURE);
    }

    // 서버에 연결
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("연결 실패");
        exit(EXIT_FAILURE);
    }

	printf("당신의 이름을 입력하세요 : ");
	fgets(name, sizeof(name), stdin);
	name[strcspn(name, "\n")] = 0;

	if(strlen(name) >= NAME_SIZE){
		fprintf(stderr, "이름은 최대 %d자까지 입력할 수 있습니다.\n", NAME_SIZE - 1);
		close(sock);
		exit(EXIT_FAILURE);
	}

    // 메시지 수신 스레드 생성
    pthread_create(&tid, NULL, receive_messages, (void *)&sock);
    pthread_detach(tid);

    // 메시지 전송
    while (1) {
      	fgets(message, BUFFER_SIZE, stdin);
		
		char message_with_name[BUFFER_SIZE];
		snprintf(message_with_name, BUFFER_SIZE, "%s: %s", name, message);
        write(sock, message_with_name, strlen(message_with_name));
		if(strncmp(message, "exit", 4) == 0){
			printf("\n%s가 접속을 종료합니다...\n", name);
			exit(0);
		}

	}

    close(sock);
    return 0;
}

