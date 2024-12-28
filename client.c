#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <signal.h>
#include <dirent.h>

#define PORT 8080
#define BUFFER_SIZE 65536
#define SHA_MAX_LENGTH 40

char *RECV_FILE = NULL;
int sockfd;
char name[256] = "서버";
char mname[256] = "클라이언트";
volatile sig_atomic_t keep_running = 1;

void handle_sigint(int sig) {
	keep_running = 0;
    printf("\n클라이언트를 강제 종료합니다.\n");
}

void file_list() {
    struct dirent *entry;
    DIR *dir = opendir("/home/woodawon/Desktop/Client");

    if (dir) {
        while((entry = readdir(dir)) != NULL) {
    	    printf("º  %s\n", entry->d_name);
		}
		closedir(dir);
    }else{
        printf("[KX_NexG] 디렉토리를 열 수 없습니다.\n");
    }
}

char *sha1sum(const char *filename) {
    
    char pszCommand[BUFFER_SIZE];
    snprintf(pszCommand, sizeof(pszCommand), "sha1sum %s", filename);

    FILE *fp = popen(pszCommand, "r");

    char pszBuff[BUFFER_SIZE];
    size_t readSize = fread(pszBuff, sizeof(char), BUFFER_SIZE-1, fp);

    pclose(fp);
    pszBuff[readSize] = '\0';

    char *hash_value = malloc(41);

    strncpy(hash_value, pszBuff, 40);
    hash_value[40] = '\0';

    return hash_value;
}

void *send_file(char sfname[]) {
    char buffer[BUFFER_SIZE];
    
    FILE *file = fopen(sfname, "rb");
    if (!file) {
        perror("[KX_NexG] 파일 열기 실패");
        pthread_exit(NULL);
    }
    
    int fsize = 0, nsize = 0, fpsize = 0;

    printf("[KX_NexG] 파일을 전송중입니다...\n");

    fseek(file, 0, SEEK_END);
    fsize = ftell(file);
    fseek(file, 0, SEEK_SET);
    

    while(nsize != fsize) {
    	fpsize = fread(buffer, 1, BUFFER_SIZE, file);
		nsize = nsize + fpsize;
		send(sockfd, buffer, fpsize, 0);
    }
 
    fclose(file);
    memset(&buffer, 0, BUFFER_SIZE);

    char *sha1_hash = sha1sum(sfname);
    snprintf(buffer, BUFFER_SIZE, "@end@hash %s", sha1_hash);
    send(sockfd, buffer, strlen(buffer), 0);
    memset(&buffer, 0, BUFFER_SIZE);
    printf("[KX_NexG] 파일 전송이 완료되었습니다.\n");
    RECV_FILE = NULL;
	return 0;
}

void *recv_file(int* count) {
    char buffer[BUFFER_SIZE];
	int nbyte;
	int chek = 1;
    FILE *file = fopen(RECV_FILE, "wb");
    char received_hash[SHA_MAX_LENGTH + 1];
    printf("[KX_NexG] 파일을 수신중입니다...\n");

    while (chek != 0) {
        nbyte = recv(sockfd, buffer, BUFFER_SIZE, 0);
	
		char *end_marker = strstr(buffer, "@end");
		if (end_marker != NULL) {
	    	fwrite(buffer, sizeof(char), end_marker - buffer, file);
	    	printf("[KX_NexG] 파일 수신이 완료되었습니다.\n");
			char *hash_marker = strstr(end_marker + 4, "@hash ");
			strcpy(received_hash, hash_marker + 6);
			received_hash[SHA_MAX_LENGTH] = '\0';
			break;
		} else {
	    	fwrite(buffer, sizeof(char), nbyte, file);
		}
    }

    fclose(fil
    char *calculated_hash = sha1sum(RECV_FILE);

    printf("[KX_NexG] 전송 파일 해시값 : %s\n", received_hash);
    printf("[KX_NexG] 전송 받은 파일 해시값 : %s\n", calculated_hash);
    
    if (strcmp(received_hash, calculated_hash) == 0) {
        printf("[KX_NexG] 파일 전송 성공: 해시 일치\n");
		*count = 0;
   		RECV_FILE = NULL; 
	} else {
        if (*count <= 5) {
            printf("[KX_NexG] 파일 전송 실패: 재전송 요청\n");
            printf("[KX_NexG] 재전송 횟수: %d\n", *count);
            memset(buffer, 0, BUFFER_SIZE);
            snprintf(buffer, sizeof(buffer), "/get %s", RECV_FILE);
            send(sockfd, buffer, strlen(buffer), 0);
            (*count)++;
	    	return 0;
        } else {
            printf("[KX_NexG] 재전송 시도 횟수: %d\n", *count);
            printf("[KX_NexG] 파일 전송 실패: 재전송 횟수가 5회를 초과했습니다. 다시 /get을 입력해주세요.\n");
            *count = 0;
            RECV_FILE = NULL;
	    	return 0;
        }
    }
    return 0;
}

void *send_message(void *arg) {
    while(1) {
		char buffer[BUFFER_SIZE];
		char rfname[128];
		char *recv_msg;
    	fgets(buffer, BUFFER_SIZE, stdin);
    	printf("\033[1A"); // 커서를 한 줄 위로 이동
		printf("\033[2K"); // 현재 줄 삭제
		buffer[strcspn(buffer, "\n")] = '\0';

    	send(sockfd, buffer, strlen(buffer), 0);
        printf("%s: %s\n", mname, buffer);
		recv_msg = strstr(buffer, "/get ");
    	if(recv_msg != NULL){
        	strcpy(rfname, recv_msg + 5); // 송신할 파일 이름
        	RECV_FILE = rfname;
	 		printf("rfname : %s\n", RECV_FILE);
        	memset(&buffer, 0, BUFFER_SIZE);
    	}
  		if(strncmp(buffer, "exit", 4) == 0) {
        	printf("[KX_NexG] 클라이언트가 종료되었습니다.\n");
	    	raise(SIGINT); // 시그널을 발생시켜 종료
    	}
		if(strncmp(buffer, "/name", 5) == 0){
			strncpy(mname, buffer + 6, sizeof(mname) -1);
			printf("\033[1A"); // 커서를 한 줄 위로 이동
			printf("\033[2K"); // 현재 줄 삭제
			printf("이름이 설정되었습니다. \n현재 이름: %s\n", mname);
		}
		if(strncmp(buffer, "service", 7) == 0) {
        	printf("[KX_NexG] 환영합니다.\n");
        	printf("[KX_NexG] 차세대 통합보안 전문기업 케이엑스넥스지 서비스를 이용해주셔서 감사드립니다.\n");
        	printf("[KX_NexG] 송・수신 가능 파일 목록을 출력합니다.\n");
        	printf("—————————————————————————————————————————————————————————————————————————————————————\n");
        	printf("*** |FILE LIST| ***");
        	file_list();
        	printf("*** |---------| ***");
    	}
	}
	pthread_exit(NULL);
}

void *recv_message(void *arg) {
    while(1) {
        char buffer[BUFFER_SIZE];
		memset(&buffer, 0, BUFFER_SIZE);
        int n = recv(sockfd, buffer, BUFFER_SIZE, 0);
        int* count = (int *)arg;
		char *send_msg;
		char *recv_msg;
		char sfname[128];
		char rfname[128];
        buffer[n] = '\0';

		send_msg = strstr(buffer, "/get ");
		recv_msg = strstr(buffer, "@start");

    	if(send_msg != NULL){
	 		strcpy(sfname, send_msg + 5); // 송신할 파일 이름
	   		printf("[KX_NexG] \"%s\" 파일을 송신합니다.\n", sfname);
        	memset(&buffer, 0, BUFFER_SIZE);
        	snprintf(buffer, sizeof(buffer), "@start");
        	send(sockfd, buffer, strlen(buffer), 0);
	 		memset(&buffer, 0, BUFFER_SIZE);
        	send_file(sfname);
    	}else if(recv_msg != NULL){
			printf("[KX_NexG] \"%s\" 파일을 수신합니다.\n", RECV_FILE);
	    	memset(&buffer, 0, BUFFER_SIZE);
        	recv_file(count);
    	}else if(strncmp(buffer, "exit", 4) == 0) {
        	printf("[KX_NexG] 서버가 종료되었으므로 클라이언트를 종료합니다.\n");
        	raise(SIGINT); // 시그널을 발생시켜 종료
    	}else if(strncmp(buffer, "/name", 5) == 0){
			printf("상대방이 이름을 설정합니다.\n");
			strncpy(name, buffer + 6, sizeof(name) - 1);
		}else{
        	printf("%s : %s\n", name, buffer);
		}
    }
    shutdown(sockfd, SHUT_RDWR); // 소켓 양방향 종료
    pthread_exit(NULL);
}

int main() {

    signal(SIGINT, handle_sigint);

    // 준비물 초기화
    int count = 0;
    struct sockaddr_in servaddr;
	char name[256];
	char buffer[256];
    
	if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("[KX_NexG] 소켓 생성 실패");
        exit(EXIT_FAILURE);
    }

    // 서버 정보 설정 및 연결 요청
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(PORT);
    servaddr.sin_addr.s_addr = inet_addr("10.200.4.180");

    if(connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("[KX_NexG] 서버와 연결 실패");
        exit(EXIT_FAILURE);
    }
	
    pthread_t send_thread, recv_thread;

    pthread_create(&send_thread, NULL, send_message, NULL);
    pthread_create(&recv_thread, NULL, recv_message, &count);
	while(1) {
        if (!keep_running) {
            const char *exit_msg = "exit";
            send(sockfd, exit_msg, strlen(exit_msg), 0);
            _exit(0);
        }
    }

    close(sockfd);
    return 0;
}
