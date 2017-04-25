#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <signal.h>

#define BACKLOG 10
#define SERVER_PORT 12345
#define CLIENT_PORT 23456

char signal_server[32];
char signal_topic[17];

void unsubscribe(int sig)
{
	int fd;
	char frame[20];
	struct hostent * he;
	struct sockaddr_in server_addr;

	he = gethostbyname(signal_server);
	fd = socket(AF_INET, SOCK_STREAM, 0);

	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(SERVER_PORT);
	server_addr.sin_addr = *((struct in_addr*)he->h_addr);
	memset(&(server_addr.sin_zero), '\0', 8);
	connect(fd,(struct sockaddr*)&server_addr, sizeof( struct sockaddr));

	memset(frame, '\0', 20);
	frame[0] = 'u';
	strncpy(frame + 1, signal_topic, 16);
	send(fd, frame, 20, 0);
	close(fd);
	exit(0);
}

int listen_sub(const char *topic)
{
	int fd;
	int sub_fd;
	int yes;
	char frame[50];
	struct sockaddr_in addr;
	struct sockaddr_in in_addr;
	socklen_t sin_size;

	fd = socket(AF_INET, SOCK_STREAM, 0);
	if(fd == -1)
	{
		printf("Funkcja socket zwróciła błąd\n");
		return -1;
	}

	if(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
	{
		printf("Funkcja setsockopt() zwróciła błąd\n");
		return -1;
	}

	addr.sin_family = AF_INET;
	addr.sin_port = htons(CLIENT_PORT);
	addr.sin_addr.s_addr = INADDR_ANY;
	memset(&(addr.sin_zero), '\0', 8);

	if(bind(fd, (struct sockaddr*)&addr, sizeof(struct sockaddr)) == -1)
	{
		printf("Funkcja bind() zwróciła błąd\n");
		return -1;
	}

	if(listen(fd, BACKLOG) == -1)
	{
		printf("Funkcja listen() zwróciła błąd\n");
		return -1;
	}

	while(1)
	{
		sin_size = sizeof(struct sockaddr_in);
		sub_fd = accept(fd, (struct sockaddr*)&in_addr, &sin_size);
		if(sub_fd == -1)
		{
			printf("Funkcja accept() zwróciła błąd\n");
			continue;
		}
		memset(frame, '\0', 50);
		recv(sub_fd, frame, 50, 0);
		printf("%s\n", frame);
	}
	return 0;
}

void handle_connection(int fd, char sub_pub, const char *topic, const char *msg)
{
	char frame[34];
	memset(frame, '\0', 17);
	if(sub_pub == 's')
	{
		frame[0] = sub_pub;
		strncpy(frame + 1, topic, 16);
		send(fd, frame, 33, 0);
		listen_sub(topic);
	}
	else if(sub_pub == 'p')
	{
		frame[0] = sub_pub;
		strncpy(frame + 1, topic, 16);
		strncpy(frame + 17, msg, 16);
		send(fd, frame, 33, 0);
	}
	else
	{
		printf("Nie prawidłowe żądanie\n");
		return;
	}
	return;
}

int main(int argc, char *argv[])
{
	int sock_fd;
	struct sockaddr_in server_addr;
	struct hostent *he;

	if(argc != 4 && argc != 5)
	{
		printf("Błędne wywołanie programu\n");
		return -1;
	}

	he = gethostbyname(argv[1]);
	if(he == NULL)
	{
		printf("Funkcja gethostbyname() zwróciła błąd\n");
		return -1;
	}

	strncpy(signal_server, argv[1], 31);
	strncpy(signal_topic, argv[3], 16);
	signal(SIGINT, unsubscribe);

	sock_fd = socket(AF_INET, SOCK_STREAM, 0);
	if(sock_fd == -1)
	{
		printf("Funkcja socket() zwróciła błąd\n");
		return -1;
	}

	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(SERVER_PORT);
	server_addr.sin_addr = *((struct in_addr*)he->h_addr);
	memset(&(server_addr.sin_zero), '\0', 8);

	if(connect(sock_fd, (struct sockaddr*)&server_addr, sizeof(struct sockaddr)) == -1)
	{
		printf("Funkcja connect() zwróciła błąd\n");
		return -1;
	}

	if(argv[2][0] == 's')
		handle_connection(sock_fd, argv[2][0], argv[3], NULL);
	else if(argv[2][0] == 'p')
		handle_connection(sock_fd, argv[2][0], argv[3], argv[4]);
	else
		printf("Nieprawidłowe żądanie\n");

	close(sock_fd);
	return 0;
}
