#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080
#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024

int client_sockets[MAX_CLIENTS];
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

void *handle_client(void *client_socket) {
    int sock = *(int *)client_socket;
    char buffer[BUFFER_SIZE];
    int bytes_read;

    while ((bytes_read = read(sock, buffer, sizeof(buffer))) > 0) {
        buffer[bytes_read] = '\0';
		printf("\n%s\n", buffer);
        // 모든 클라이언트에게 메시지를 전송
        pthread_mutex_lock(&clients_mutex);
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (client_sockets[i] != 0 && client_sockets[i] != sock) {
                write(client_sockets[i], buffer, bytes_read);
            }
        }
        pthread_mutex_unlock(&clients_mutex);
    }

    // 클라이언트 연결 종료 시
    close(sock);
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_sockets[i] == sock) {
            client_sockets[i] = 0;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);

    return NULL;
}

void *send_messages(void *arg) {
    char message[BUFFER_SIZE];
    
    while (1) {
        fgets(message, BUFFER_SIZE, stdin);
		
		char message_with_name[BUFFER_SIZE];
		snprintf(message_with_name, BUFFER_SIZE, "서버: %s", message);

        // 모든 클라이언트에게 서버의 메시지를 전송
        pthread_mutex_lock(&clients_mutex);
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (client_sockets[i] != 0) {
                write(client_sockets[i], message_with_name, strlen(message_with_name));
            }
        }
        pthread_mutex_unlock(&clients_mutex);
    }

    return NULL;
}

int main() {
    int server_socket, new_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);
    pthread_t tid, send_tid;

    // 서버 소켓 생성
    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("소캣 생성 실패");
        exit(EXIT_FAILURE);
    }

    // 서버 주소 설정
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    // 서버 소켓에 주소 바인딩
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("바인딩 실패");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    // 연결 대기
    if (listen(server_socket, 3) < 0) {
        perror("리슨 실패..ㅠㅠ");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    printf("서버 연결 포트 %d\n", PORT);

    // 서버에서 메시지 전송 스레드 시작
    pthread_create(&send_tid, NULL, send_messages, NULL);

    // 클라이언트 처리
    while (1) {
        if ((new_socket = accept(server_socket, (struct sockaddr *)&client_addr, &addr_len)) < 0) {
            perror("수락 실패");
            continue;
        }

        // 클라이언트 소켓 저장
        pthread_mutex_lock(&clients_mutex);
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (client_sockets[i] == 0) {
                client_sockets[i] = new_socket;
                break;
            }
        }
        pthread_mutex_unlock(&clients_mutex);

        // 클라이언트 처리를 위한 스레드 생성
        pthread_create(&tid, NULL, handle_client, (void *)&new_socket);
        pthread_detach(tid);
    }

    return 0;
}
