#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <openssl/sha.h>  // SHA-256 사용
#include <signal.h> // 시그널 관련 헤더 포함

#define PORT 8080
#define BUFFER_SIZE 65536
#define SIZE 256
#define START "@start"
#define END "@end"
#define HASH "@hash"
#define GET "/get"
#define EXIT "exit"
#define NAME "/name"

struct sockaddr_in servaddr, cliaddr;

// sockfd와 count를 포함한 구조체 정의
typedef struct {
    int sockfd;
    int count;
	char filename[SIZE];
} ConnectionInfo;

char name[SIZE] = "클라이언트";
char mname[SIZE] = "서버";

volatile sig_atomic_t keep_running = 1;

void handle_sigint(int sig) {
	keep_running = 0;
	printf("\nSIGINT 시그널을 받았습니다. 서버를 종료합니다...\n");
}

void calculate_sha1(FILE *file, char *hash_string) {
    SHA_CTX sha1;
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1_Init(&sha1);
    unsigned char buffer[BUFFER_SIZE];
    int bytesRead = 0;
    rewind(file);
    while ((bytesRead = fread(buffer, 1, BUFFER_SIZE, file)) > 0) {
        SHA1_Update(&sha1, buffer, bytesRead);
    }
    SHA1_Final(hash, &sha1);
    for (int i = 0; i < SHA_DIGEST_LENGTH; i++) {
        sprintf(&hash_string[i * 2], "%02x", hash[i]);
    }
}

void *send_file(void *arg) {
    ConnectionInfo *conn = (ConnectionInfo *)arg;
    char buffer[BUFFER_SIZE];
    size_t fsize, nsize = 0;
    FILE *file = fopen(conn->filename, "rb");
	fseek(file, 0 , SEEK_END);
	fsize=ftell(file);
	fseek(file, 0, SEEK_SET);
	char sha1_hash[SHA_DIGEST_LENGTH * 2 + 1];
	while(nsize!=fsize){
		int fpsize = fread(buffer, 1, BUFFER_SIZE, file);
		nsize += fpsize;
    	send(conn->sockfd, buffer, fpsize, 0);
	}

    calculate_sha1(file, sha1_hash);

    memset(&buffer, 0, BUFFER_SIZE);
    snprintf(buffer, BUFFER_SIZE, "%s%s %s",END, HASH, sha1_hash);
    send(conn->sockfd, buffer, BUFFER_SIZE, 0);
    fclose(file);
    printf("파일 전송이 완료되었습니다.\n");
	memset(&conn->filename, 0, sizeof(conn->filename));
	return 0;
}

void *recv_file(void *arg) {
    ConnectionInfo *conn = (ConnectionInfo *)arg;
    int *count = &conn->count;
	char buffer[BUFFER_SIZE];
    int nbyte = SIZE;
    FILE *file = fopen(conn->filename, "wb+");
    char received_hash[SHA_DIGEST_LENGTH * 2 + 1];
    char calculated_hash[SHA_DIGEST_LENGTH * 2 + 1];
    while (nbyte!=0) {
		nbyte = recv(conn->sockfd, buffer, BUFFER_SIZE, 0);
        char *end_marker = strstr(buffer, END);
		if (end_marker != NULL) {
            fwrite(buffer, sizeof(char), end_marker - buffer, file);
            char *hash_marker = strstr(end_marker + 4, HASH);
			if (hash_marker != NULL) {
                strcpy(received_hash, hash_marker + 6);
                received_hash[SHA_DIGEST_LENGTH * 2] = '\0';
        		break;
			}
        } else {
            fwrite(buffer, sizeof(char), nbyte, file);
        }
	}
    
	fflush(file);
    fseek(file, 0, SEEK_SET);
    calculate_sha1(file, calculated_hash);
	fclose(file);
	

    if (strcmp(received_hash, calculated_hash) == 0) {
    	printf("파일 전송 성공: 해시 일치\n");
    	memset(&conn->filename, 0, sizeof(conn->filename));
		*count = 0;
	} else {
        if (*count <= 5) {
            printf("파일 전송 실패: 재전송 요청\n재전송 횟수: %d\n", *count);
            memset(buffer, 0, BUFFER_SIZE);
            snprintf(buffer, BUFFER_SIZE, "%s %s", GET, conn->filename);
			send(conn->sockfd, buffer, strlen(buffer), 0);
			(*count)++;
			remove(conn->filename);
			return 0;
        } else {
            *count = 0;
    		remove(conn->filename);
            return 0;
        }
    }
    return 0;
}

void *send_message(void *arg) {
	ConnectionInfo *conn = (ConnectionInfo *)arg;
    while (1) { // 종료 플래그 확인    
		char buffer[BUFFER_SIZE];
   		fgets(buffer, BUFFER_SIZE, stdin);
		printf("\033[1A"); // 커서를 한 줄 위로 이동
	    printf("\033[2K"); // 현재 줄 삭제
		buffer[strcspn(buffer, "\n")] = '\0';
        send(conn->sockfd, buffer, strlen(buffer), 0);
		printf("%s: %s\n", mname, buffer);
		if(strncmp(buffer, GET, 4) == 0){
			strncpy(conn->filename, buffer + 5, sizeof(conn->filename) - 1);
		}
		if(strncmp(buffer, NAME, 5) == 0){
			strncpy(mname, buffer + 6, sizeof(mname) -1);
			printf("\033[1A"); // 커서를 한 줄 위로 이동
			printf("\033[2k"); // 현재 줄 삭제
			printf("이름이 설정되었습니다. \n현재 이름: %s\n", mname);
		}
		if(strncmp(buffer, "exit", 4) == 0) {
            printf("서버를 종료합니다.\n");
        	exit(0);
		}
    }
    pthread_exit(NULL);
}

void *recv_message(void *arg) {
    ConnectionInfo *conn = (ConnectionInfo *)arg;
	
	while (1) { // 종료 플래그 확인
		char buffer[BUFFER_SIZE];
        int n = recv(conn->sockfd, buffer, BUFFER_SIZE, 0);
        buffer[n] = '\0';
		if(strncmp(buffer, GET, 4) == 0){
            strncpy(conn->filename, buffer + 5, sizeof(conn->filename) - 1);
			printf("파일을 전송합니다.\n");
			memset(&buffer, 0, BUFFER_SIZE);
			snprintf(buffer, BUFFER_SIZE, "%s %s",START, conn->filename);
            send(conn->sockfd, buffer, BUFFER_SIZE, 0);
			send_file(arg);
		} else if(strncmp(buffer, START, 6) == 0){
			printf("파일을 수신합니다.\n");
			recv_file(arg);
		}else if(strncmp(buffer, NAME, 5) == 0){
			printf("상대방이 이름을 설정하였습니다.\n");
			strncpy(name, buffer + 6, sizeof(name) - 1);
		}else {
            printf("%s : %s\n",name, buffer);
		}
		if(strncmp(buffer, "exit", 4) == 0) {
            printf("서버가 종료되었으므로 클라이언트를 종료합니다.\n");
        	_exit(0);
		}
	}
    pthread_exit(NULL);
}

int main() {
	signal(SIGINT, handle_sigint);
	int sockfd;
    socklen_t clilen = sizeof(cliaddr);
    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("소켓 생성 실패");
        exit(EXIT_FAILURE);
    }

	// 소켓 옵션 설정 (SO_REUSEADDR)
	int opt = 1;
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
    	perror("소켓 옵션 설정 실패");
    	close(sockfd);
    	exit(EXIT_FAILURE);
	}
	
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(PORT);
    servaddr.sin_addr.s_addr = INADDR_ANY;

    if(bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("바인딩 실패");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    if(listen(sockfd, 1) < 0) {
        perror("서버 대기 실패");
        close(sockfd);
        exit(EXIT_FAILURE);
    } 

    int client_sockfd = accept(sockfd, (struct sockaddr *)&cliaddr, &clilen);
	if(client_sockfd < 0) {
        perror("클라이언트와 연결 실패");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    pthread_t send_thread, recv_thread;
    
    ConnectionInfo conn;
    conn.sockfd = client_sockfd;
    conn.count = 0;
	pthread_create(&send_thread, NULL, send_message, &conn);
    pthread_create(&recv_thread, NULL, recv_message, &conn);
    while(1) {
		if (!keep_running) {
    		send(client_sockfd, EXIT, strlen(EXIT), 0);
			_exit(0);
		}
    }
    return 0;
}
