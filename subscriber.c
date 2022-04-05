#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/tcp.h>

struct tcp_msg{
	char ip[16];
	uint16_t udp_port;
	char topic[51];
	char type[11];
	char content[1501];
};

struct command_subscribe{
	char type;
    char topic[51];
    char SF;
};

struct command_unsubscribe{
	char type;
    char topic[51];
};

void usage(char *file)
{
	fprintf(stderr, "Usage: %s server_address server_port\n", file);
	exit(0);
}

int main(int argc, char *argv[])
{
	setvbuf(stdout, NULL, _IONBF, BUFSIZ);
	int sockfd, n, ret, BUFLEN = sizeof(struct tcp_msg);
	struct sockaddr_in serv_addr;
	char buffer[BUFLEN];
	fd_set read_fds, tmp_fds;
	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);
	int fdmax;

	if (argc < 4) {
		usage(argv[0]);
	}

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 0){
        perror("Socker error");
        exit(0);
    }

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(atoi(argv[3]));
	ret = inet_aton(argv[2], &serv_addr.sin_addr);
	if(ret == 0){
        perror("Inet_aton error");
        exit(0);
    }

    //trimitem cerere de conexiune la server
	ret = connect(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr));
    if(ret < 0){
        perror("Connect error");
        exit(0);
    }

    //trimitem id-ul pe care vrem sa il utilizam
    ret = send(sockfd, argv[1], strlen(argv[1]) + 1, 0);
    if(ret < 0){
        perror("Failed to send ID to server");
        exit(0);
    }

    //dezactivam algoritmul Nagle
    int flag = 1;
    setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(int));

	FD_SET(STDIN_FILENO, &read_fds);
	FD_SET(sockfd, &read_fds);
	fdmax = sockfd;

	while (1) {
		tmp_fds = read_fds; 
		
		ret = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
		if(FD_ISSET(STDIN_FILENO, &tmp_fds)){
			// se citeste de la tastatura
			memset(buffer, 0, BUFLEN);
			fgets(buffer, BUFLEN - 1, stdin);

			if (strncmp(buffer, "exit", 4) == 0) {
				break;
			}

			//comanda nu a fost exit
            if(strncmp(buffer, "subscribe", 9) == 0){
                struct command_subscribe temp_subscribe;
				buffer[strlen(buffer) - 1] = 0;
				temp_subscribe.type = 's';
                char *token;
                token = strtok(buffer, " ");
                token = strtok(NULL, " ");
                sprintf(temp_subscribe.topic, "%s", token);
                token = strtok(NULL, " ");
                temp_subscribe.SF = token[0];
				ret = send(sockfd, (char *)&temp_subscribe, sizeof(temp_subscribe), 0);
				if(ret < 0){
					//printf("Subscribe error\n");
				} else {
					printf("Subscribed to topic.\n");
				}
            } else if(strncmp(buffer, "unsubscribe", 11) == 0){
				struct command_unsubscribe temp_unsub;
				buffer[strlen(buffer) - 1] = 0;
				temp_unsub.type = 'u';
				char *token;
				token = strtok(buffer, " ");
				token = strtok(NULL, " ");
				sprintf(temp_unsub.topic, "%s", token);
				ret = send(sockfd, (char *)&temp_unsub, sizeof(temp_unsub), 0);
				if(ret < 0){
					//printf("Unsubscribe error\n");
				} else {
					printf("Unsubscribed from topic.\n");
				}
			}
		}
		if(FD_ISSET(sockfd, &tmp_fds)){
			memset(buffer, 0, BUFLEN);
			n = recv(sockfd, buffer, sizeof(buffer), 0);
			if(n < 0){
				perror("Receive error");
				exit(0);
			}
			if(!strcmp(buffer, "kick")){
				break;
			}
			struct tcp_msg *received = (struct tcp_msg *)buffer;
			printf("%s:%hu - %s - %s - %s\n", received->ip, received->udp_port,
                   received->topic, received->type, received->content);
		}
	}

	close(sockfd);

	return 0;
}
