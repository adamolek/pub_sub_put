#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define PORT 12345
#define BACKLOG 10

void handle_connection(int fd, struct sockaddr_in addr)
{
	return;
}

int main()
{
	int sock_fd, in_fd;
	struct sockaddr_in addr;
	struct sockaddr_in in_addr;
	socklen_t sin_size;
	int yes = 1;

	sock_fd = socket(AF_INET, SOCK_STREAM, 0);
	if(sock_fd == -1)
	{
		printf("Funkcja socket() zwróciała błąd\n");
		return -1;
	}

	if(setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
	{
		printf("Funkcja setsockopt() zwróciła błąd\n");
		return -1;
	}

	addr.sin_family = AF_INET;
	addr.sin_port = htons(PORT);
	addr.sin_addr.s_addr = INADDR_ANY;
	memset(&(addr.sin_zero), '\0', 8);

	if(bind(sock_fd, (struct sockaddr*)&addr, sizeof(struct sockaddr)) == -1)
	{
		printf("Funkcja bind() zwróciła błąd\n");
		return -1;
	}

	if(listen(sock_fd, BACKLOG) == -1)
	{
		printf("Funkcja listen() zwróciła błąd\n");
		return -1;
	}

	while(1)
	{
		sin_size = sizeof(struct sockaddr_in);
		in_fd = accept(sock_fd, (struct sockaddr*)&in_addr, &sin_size);
		if(in_fd == -1)
		{
			printf("Funkcja accept() zwróciła błąd\n");
			continue;
		}
		handle_connection(in_fd, in_addr);
	}

	return 0;
}
