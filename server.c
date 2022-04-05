#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <math.h>
#include <netinet/tcp.h>

int added_Client = 0;

typedef struct command_subscribe{
	char type;
    char topic[51];
    char SF;
}cmd_sub;

typedef struct command_unsubscribe{
	char type;
    char topic[51];
}cmd_unsub;

struct tcp_msg{
	char ip[16];
	uint16_t udp_port;
	char topic[51];
	char type[11];
	char content[1501];
};

typedef struct msg_to_send{
	char ip[16];
	uint16_t udp_port;
	char topic[51];
	char type[11];
	char content[1501];
	struct msg_to_send *next;
}msg_L;

typedef struct pending_messages{
	char *id;
	int fd;
	msg_L *pending_messages;
	struct pending_messages *next;
}pending_L;

//functie care aloca memorie pentru un mesaj de tip msg_L
msg_L *allocate_message(){
	msg_L *node = NULL;
	node = (msg_L *)malloc(sizeof(msg_L));
	return node;
}

//functie care adauga un mesaj in o lista de mesaje msg_L
void add_message(msg_L *msg_list, msg_L *new_message){
	msg_L *current = msg_list;
	while(current != NULL){
		current = current->next;
	}
	current = new_message;
	return;
}

typedef struct topic_list{
    char topic[51];
    int SF;
    struct topic_list *next;
}topic_L;

typedef struct client_list{
	int fd;
    char *id;
    int connected;
    topic_L *subscriptions;
    struct client_list *next;
}client_L;

void usage(char *file)
{
	fprintf(stderr, "Usage: %s server_port\n", file);
	exit(0);
}

//functie care aloca memorie pentru un client
client_L *allocate_client(char *id, int fd){
    client_L *node = NULL;
    node = (client_L *)malloc(sizeof(client_L));
	node->id = (char *)malloc(sizeof(char) * sizeof(id));
	if(node == NULL){
		perror("Unable to allocate memory for client");
		exit(0);
	}
	strcat(node->id, id);
	node->fd = fd;
	node->connected = 1;
    return node;
}

//functie care aloga memorie pentru subscriptiile unui client
topic_L *allocate_subscriptions(){
    topic_L *node = NULL;
    node = (topic_L *)malloc(sizeof(topic_L));
	node->next = NULL;
    return node;
}

//functie care verifica daca o lista de clienti este goala
int client_list_isEmpty(client_L *list)
{
    if(list == NULL){
        return 1;
	}
    return 0;
}

//functie care cauta si returneaza un client prin fd
client_L *search_client_by_fd(client_L *list, int fd)
{
	client_L *current = list;
	while(current != NULL){
		if(current->fd == fd){
			return current;
		}
		current = current->next;
	}
	return NULL;
}

//functie care cauta si returneaza un client prin id
client_L *search_client_by_id(client_L *list, char *id)
{
	client_L *current = list;
	while(current != NULL){
		if(!strcmp(current->id, id)){
			return current;
		}
		current = current->next;
	}
	return NULL;
}

//functie care verifica daca un client este deja conectat cu id-ul nou trimis in functie. Daca
//nu a fost pana acum folosit ii aloca memorie pentru un client nou, daca a fost folosit, dar
//nu este conectat il conecteaza.
client_L *connect_client(client_L *list ,char *id, int fd)
{
    if(client_list_isEmpty(list)){
        list = allocate_client(id, fd);
		added_Client = 1;
    } else {
        client_L *current = list;
		if(strcmp(current->id, id) == 0 && current->connected == 0){
			current->connected = 1;
			current->fd = fd;
			added_Client = 1;
            return list;
		} else if(strcmp(current->id, id) == 0 && current->connected == 1){
			return list;
		}
        while(current->next != NULL){
            if(strcmp(current->next->id, id) == 0 && current->next->connected == 1){
				return list;
            } else if(strcmp(current->next->id, id) == 0 && current->next->connected == 0) {
                current->next->connected = 1;
				current->next->fd = fd;
				added_Client = 1;
                return list;
            }
            current = current->next;
        }
        current->next = allocate_client(id, fd);
        current->next->next = NULL;
		added_Client = 1;
    }
	return list;
}

//functie care deconecteaza un client din lista de clienti
client_L *disconnect_client(client_L *list,int fd)
{
    client_L *current = list;
    while(current != NULL){
        if(current->fd == fd){
            current->connected = 0;
            return current;
        }
		current = current->next;
    }
	return NULL;
}

//functie care printeaza lista de clienti
//(folosita la debug)
void print_clients(client_L *clients){
	client_L *current = clients;
	while(current != NULL){
		printf("ID: %s Connected: %d\n", current->id, current->connected);
		current = current->next;
	}
}

//functie care printeaza mesajele care inca nu au fost trimise
//(folosita la debug)
void print_pending(pending_L *pending){
	pending_L *current = pending;
	while(current != NULL){
		printf("%s: ", current->id);
		msg_L *current_msg = current->pending_messages;
		if(current_msg == NULL){
			printf("NULL");
		}
		while(current_msg != NULL){
			printf("%s %s %s, ", current_msg->topic, current_msg->type, current_msg->content);
			current_msg = current_msg->next;
		}
		current = current->next;
		printf("\n");
	}
}

//functie care printeaza lsita de subscriptions a unui client
//(folosita la debug)
void print_subscriptions(topic_L *subscriptions){
	topic_L *current = subscriptions;
	while(current != NULL){
		printf("%s %d\n", current->topic, current->SF);
		current = current->next;
	}
}

//functie care verifica daca lista de subscriptions a unui client este goala
int subscriptions_areEmpty(topic_L *list)
{
    if(list == NULL){
        return 1;
    }
    return 0;
}


//functie care adauga un topic in lista de subscriptions a unui client
topic_L *add_subscription(topic_L *list, char *topic, int SF)
{
	if(subscriptions_areEmpty(list)){
        list = allocate_subscriptions();
		strcpy(list->topic, topic);
		list->SF = SF;
		return list;
    } else {
        topic_L *current = list;
        while(current->next != NULL){
            if(strcmp(current->next->topic, topic) == 0){
                printf("Topic %s already subscribed to\n", topic);
            }
            current = current->next;
        }
        current->next = allocate_subscriptions();
        strcpy(current->next->topic, topic);
        current->next->SF = SF;
        current->next->next = NULL;
		return list;
    }
}

//functie care scoate un topic din lista de subscriptions a unui client
topic_L *unsubscribe(topic_L *list, char *topic)
{
	topic_L *current = list;
	if(strcmp(current->topic, topic) == 0){
		topic_L *temp = current;
		free(temp);
		return current->next;
	}
    while(current != NULL){
        if(strcmp(current->next->topic, topic) == 0){
			topic_L *temp = current->next;
            current->next = current->next->next;
			free(temp);
            return list;
        }
    }
}

//functie care deazloca memoria clientilor
void free_all(client_L *clients)
{
	client_L *current_client = clients;
	while(current_client != NULL){
		client_L *temp_client = current_client;
		topic_L *current_topic = current_client->subscriptions;
		while(current_topic != NULL)
		{
			topic_L *temp_topic = current_topic;
			current_topic = current_topic->next;
			free(temp_topic);
		}
		current_client = current_client->next;
		free(temp_client);
	}
}

//functie care aloca un element de tip msg_L
msg_L *allocate_storage(struct tcp_msg *message)
{
	msg_L *node = (msg_L *)malloc(sizeof(msg_L));
	strcpy(node->ip, message->ip);
	node->udp_port = message->udp_port;
	strcpy(node->content, message->content);
	strcpy(node->topic, message->topic);
	strcpy(node->type, message->type);
	node->next = NULL;
	return node;
}

//functie care verifica daca un element de tip msg_L este gol
int storage_is_empty(msg_L *storage)
{
	if(storage == NULL){
		return 1;
	}
	return 0;
}

//functie care aloca un element de tip pending
pending_L *allocate_pending(struct tcp_msg *message, client_L *client){
	pending_L *node = (pending_L *)malloc(sizeof(pending_L));
	node->pending_messages = allocate_storage(message);
	node->id = client->id;
	node->fd = client->fd;
	node->next = NULL;
	return node;
}

//functie care adauga un mesaj in lista de pending a client-ului dat ca parametru. Se cauta
//id-ul client-ului in lista de pending-uri si apoi in functie daca este gasit sau nu ii adauga
//un nou element de tip pending in lista de pending sau adauga mesajul in lista de pending deja existenta
pending_L *add_pending_message(pending_L *pending, struct tcp_msg *message, client_L *client)
{
	pending_L *current_pending = pending;
	if(current_pending == NULL){
		pending = allocate_pending(message, client);
		return pending;
	}
	int exists = 0;
	if(!strcmp(current_pending->id, client->id)){
		msg_L *current_pending_message = current_pending->pending_messages;
		if(current_pending_message == NULL){
		current_pending->pending_messages = allocate_storage(message);
		} else {
			while(current_pending_message->next != NULL){
				current_pending_message = current_pending_message->next;
			}
			current_pending_message->next = allocate_storage(message);
		}
	} else {
		while(current_pending->next != NULL){
			if(!strcmp(current_pending->next->id, client->id)){
				exists = 1;
				break;
			}
			current_pending = current_pending->next;
		}
		if(exists == 1){
			msg_L *current_pending_message = current_pending->next->pending_messages;
			if(current_pending_message == NULL){
				current_pending->next->pending_messages = allocate_storage(message);
			} else {
				while(current_pending_message->next != NULL){
					current_pending_message = current_pending_message->next;
				}
				current_pending_message->next = allocate_storage(message);
			}
		} else {
			current_pending->next = allocate_pending(message, client);
		}
	}
	return pending;
}

//functie care scoate un mesaj dintr-un element de tip pending
pending_L *remove_pending_message(pending_L *pending, struct tcp_msg *message)
{
	msg_L *current_msg = pending->pending_messages;
	if(!strcmp(current_msg->topic, message->topic)){
		msg_L *temp = current_msg;
		pending->pending_messages = current_msg->next;
		free(temp);
		return pending;
	}
	while(current_msg->next != NULL){
		if(!strcmp(current_msg->next->topic, message->topic)){
			msg_L *temp = current_msg->next;
			current_msg->next = current_msg->next->next;
			free(temp);
		}
	}
	return pending;
}


//functie care cauta toti clientii care sunt abonati la topic-ul mesajului trimis in functie
//in cazul in care gaseste un astfel de client, verifica daca acesta este conectat si apoi ii trimite
// mesajul. Daca nu este conectat, verifica daca acest client are SF = 1, in acest caz ii stocheaza 
//mesajul in lista de pending-uri
pending_L *send_or_store(struct tcp_msg *message, client_L *clients, pending_L *pending)
{
	client_L *current_client = clients;
	while(current_client != NULL){
		topic_L *current_sub = current_client->subscriptions;
		while(current_sub != NULL){
			if(!strcmp(message->topic, current_sub->topic)){
				if(current_client->connected){
					int ret;
					ret = send(current_client->fd, (char*) message, sizeof(struct tcp_msg), 0);
					if(ret < 0){
						perror("Sending error");
						exit(0);
					}
				} else if(current_sub->SF){
					pending = add_pending_message(pending, message, current_client);
				}
			}
			current_sub = current_sub->next;
		}
		current_client = current_client->next;
	}
	return pending;
}

//functie care cauta in lista de pending, compara id-ul clientul-ui cu cel din lista de pending,
//daca acestea coincid incep sa trimit toate mesajele netrimise din lista de pending.
pending_L *search_in_pending(char *id, pending_L *pending, client_L *clients){
	pending_L *current_pending = pending;
	client_L *temp_cli = search_client_by_id(clients, id);
	while(current_pending != NULL){
		if(!strcmp(current_pending->id, temp_cli->id)){
			msg_L *current_msg = current_pending->pending_messages;
			while(current_msg != NULL){
				struct tcp_msg temp;
				int ret;
				strcpy(temp.content, current_pending->pending_messages->content);
				strcpy(temp.ip, current_pending->pending_messages->ip);
				strcpy(temp.topic, current_pending->pending_messages->topic);
				strcpy(temp.type, current_pending->pending_messages->type);
				temp.udp_port = current_pending->pending_messages->udp_port;
				ret = send(temp_cli->fd, (char *)&temp, sizeof(struct tcp_msg), 0);
				if(ret < 0){
					perror("Sending error");
					exit(0);
				}
				current_pending = remove_pending_message(current_pending, &temp);
				current_msg = current_pending->pending_messages;
			}
		} 
		current_pending = current_pending->next;
	}
	return pending;
}

int main(int argc, char *argv[])
{
	setvbuf(stdout, NULL, _IONBF, BUFSIZ);
	int BUFLEN = sizeof(struct tcp_msg);
	int newsockfd, portno, udp_sockfd, tcp_sockfd;
	char buffer[BUFLEN];
	struct sockaddr_in serv_addr, cli_addr, udp_serv_addr, tcp_serv_addr;
	int n, i, ret;
	socklen_t clilen;
	client_L *clients = NULL;
	msg_L *storage = NULL;
	pending_L *pending = NULL;

	fd_set read_fds;	// multimea de citire folosita in select()
	fd_set tmp_fds;		// multime folosita temporar
	int fdmax;			// valoare maxima fd din multimea read_fds

	if (argc < 2) {
		usage(argv[0]);
	}

	// se goleste multimea de descriptori de citire (read_fds) si multimea temporara (tmp_fds)
	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);

	//set_up_clients(clients);

	//creeam socket udp
	udp_sockfd = socket(PF_INET, SOCK_DGRAM, 0);
	if(udp_sockfd < 0){
		perror("UDP socket error");
		exit(0);
	}


	//creeam socket tcp
	tcp_sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if(tcp_sockfd < 0){
		perror("TPC socket error");
		exit(0);
	}

	portno = atoi(argv[1]);
	if(portno < 0){
		perror("Atoi error");
		exit(0);
	}

	memset((char *) &udp_serv_addr, 0, sizeof(udp_serv_addr));
	memset((char*) &tcp_serv_addr, 0, sizeof(tcp_serv_addr));
	udp_serv_addr.sin_family = tcp_serv_addr.sin_family = AF_INET;
	udp_serv_addr.sin_port = tcp_serv_addr.sin_port = htons(portno);
	udp_serv_addr.sin_addr.s_addr = tcp_serv_addr.sin_addr.s_addr = INADDR_ANY;

	ret = bind(udp_sockfd, (struct sockaddr *) &udp_serv_addr, sizeof(struct sockaddr));
	if(ret < 0){
		perror("UDP bind error");
		exit(0);
	}

	ret = bind(tcp_sockfd, (struct sockaddr *) &tcp_serv_addr, sizeof(struct sockaddr));
	if(ret < 0){
		perror("TCP bind error");
		exit(0);
	}

	ret = listen(tcp_sockfd, __INT_MAX__);
	if(ret < 0){
		perror("Listen error");
		exit(0);
	}

	// se adauga noul file descriptor (socketul pe care se asculta conexiuni) in multimea read_fds
	FD_SET(tcp_sockfd, &read_fds);
	FD_SET(udp_sockfd, &read_fds);
	//adaugam si 0 pentru citirea de la tastatura
	FD_SET(STDIN_FILENO, &read_fds);
	fdmax = tcp_sockfd;

	while (1) {
		tmp_fds = read_fds; 
		ret = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
		if(ret < 0){
			perror("Select error");
			exit(0);
		}

		for (i = 0; i <= fdmax; i++) {
			if (FD_ISSET(i, &tmp_fds)) {
				if(i == STDIN_FILENO){
					//am primit un mesaj de la stdin
					if(fgets(buffer, BUFLEN - 1, stdin) != NULL){
						if(strcmp(buffer, "exit\n") == 0){
							client_L *temp = clients;
							while(temp != NULL){
								if(temp->connected == 1){
									send(temp->fd, "kick", sizeof("kick"), 0);
									close(temp->fd);
								}
								temp = temp->next;
							}
							close(tcp_sockfd);
							close(udp_sockfd);
							free_all(clients);
							return 0;
						} else {
							//printf("Only exit command is allowed\n");
						}
					}
				} else if(i == udp_sockfd){
					//am primit mesaj de la un UDP
					socklen_t udp_socklen = sizeof(udp_serv_addr);
					recvfrom(udp_sockfd, buffer, BUFLEN, 0, (struct sockaddr*) &udp_serv_addr, &udp_socklen);
					char topic[50];
					unsigned int type;
					char content[1500];
					memcpy(&topic, buffer, 50);
					memcpy(&type, buffer+50, 1);
					memcpy(&content, buffer+51, 1501);
					struct tcp_msg *temp = (struct tcp_msg*)malloc(sizeof(struct tcp_msg));
					switch (type)
					{
					case 0: ;
						long long int_num;
						int_num = ntohl(*(uint32_t*)(content + 1));
						if(content[0]){
							int_num *= -1;
						}
						temp->udp_port = ntohs(udp_serv_addr.sin_port);
                    	strcpy(temp->ip, inet_ntoa(udp_serv_addr.sin_addr));
						strncpy(temp->topic, topic, 50);
						sprintf(temp->content, "%lld", int_num);
						strcpy(temp->type, "INT");
						break;
					
					case 1: ;
						double short_num;
						short_num = ntohs(*(uint16_t *)(content));
						short_num = short_num/100;
						temp->udp_port = ntohs(udp_serv_addr.sin_port);
                    	strcpy(temp->ip, inet_ntoa(udp_serv_addr.sin_addr));
						strncpy(temp->topic, topic, 50);
						sprintf(temp->content, "%.2f", short_num);
						strcpy(temp->type, "SHORT_REAL"); 
						break;

					case 2: ;
						double float_num;
						float_num = ntohl(*(uint32_t *)(content + 1));
						float_num = float_num/pow(10, content[5]);

						if(content[0]){
							float_num *= -1;
						}
						temp->udp_port = ntohs(udp_serv_addr.sin_port);
                    	strcpy(temp->ip, inet_ntoa(udp_serv_addr.sin_addr));
						strncpy(temp->topic, topic, 50);
						sprintf(temp->content, "%lf", float_num);
						strcpy(temp->type, "FLOAT"); 
						break;

					default:
						temp->udp_port = ntohs(udp_serv_addr.sin_port);
                    	strcpy(temp->ip, inet_ntoa(udp_serv_addr.sin_addr));
						strncpy(temp->topic, topic, 50);
						strcpy(temp->content, content);
						strcpy(temp->type, "STRING"); 
						break;
					}
					pending = send_or_store(temp, clients, pending);	

				} else if(i == tcp_sockfd) {
					// a venit o cerere de conexiune pe socketul inactiv (cel cu listen),
					// pe care serverul o accepta
					clilen = sizeof(cli_addr);
					newsockfd = accept(tcp_sockfd, (struct sockaddr *) &cli_addr, &clilen);
					if(newsockfd < 0){
						//printf("Accept error");
					}

					//setam socket cu TCP_NODELAY pentru a da disable la algoritmul Nagle
					int flag = 1;
					setsockopt(newsockfd, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, sizeof(int));
					// se adauga noul socket intors de accept() la multimea descriptorilor de citire
					FD_SET(newsockfd, &read_fds);
					if (newsockfd > fdmax) { 
						fdmax = newsockfd;
					}
					//primim id-ul cu care clientul vrea sa se conecteze
					n = recv(newsockfd, buffer, sizeof(buffer), 0);
					if(n < 0){
						//printf("No ID given");
					}
					//verificam daca putem conecta clientul, daca se poate conecta se modifica variabila globala added_Client
					clients = connect_client(clients, buffer, newsockfd);
					if(added_Client == 1){
						printf("New client %s connected from %s:%d.\n", buffer, inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
						pending = search_in_pending(buffer, pending, clients);
						added_Client = 0;
					} else {
						printf("Client %s already connected.\n", buffer);
						send(newsockfd, "kick", sizeof("kick"), 0);
						close(newsockfd);
						FD_CLR(newsockfd, &read_fds);
					}
				} else {
					// s-au primit date pe unul din socketii de client,
					// asa ca serverul trebuie sa le receptioneze
					memset(buffer, 0, BUFLEN);
					n = recv(i, buffer, sizeof(buffer), 0);
					if(n < 0){
						perror("Recv error");
						exit(0);
					}

					if (n == 0) {
						// un client a iesit
						client_L *dclient = disconnect_client(clients, i);
						printf("Client %s disconnected.\n", dclient->id);
						close(i);						
						// se scoate din multimea de citire socketul inchis 
						FD_CLR(i, &read_fds);
					} else {
						if(buffer[0] == 's'){
							cmd_sub *temp_msg = (cmd_sub*)buffer;
							client_L *temp_client = search_client_by_fd(clients, i);
							int SF = temp_msg->SF - '0';
							temp_client->subscriptions = add_subscription(temp_client->subscriptions, temp_msg->topic, SF);
						} else if(buffer[0] == 'u'){
							cmd_unsub *temp_msg = (cmd_unsub*)buffer;
							client_L *temp_client = search_client_by_fd(clients, i);
							temp_client->subscriptions = unsubscribe(temp_client->subscriptions, temp_msg->topic);
						}
					}
				}
			}
		}
	}
	close(tcp_sockfd);
	close(udp_sockfd);
	free_all(clients);

	return 0;
}
