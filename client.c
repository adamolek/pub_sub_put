#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>

#define BACKLOG 10
#define SERVER_PORT 12345
#define CLIENT_PORT 23456

int listen_sub(const char *topic)
{
	int fd;
	int sub_fd;
	int yes;
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
		//dodać odczyt komunikatów itp

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
		printf("Publikacja\n");
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

	if(argc != 4)
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

	handle_connection(sock_fd, argv[2][0], argv[3], NULL);
	close(sock_fd);
	return 0;
}
