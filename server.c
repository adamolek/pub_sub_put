#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <pthread.h>
#include <sys/file.h>
#include <arpa/inet.h>

#define PORT 12345
#define BACKLOG 10
#define DIRECTORY "/tmp/pub_sub"

struct sub_req_data
{
	struct sockaddr_in client;
	char topic[17];
};

struct pub_req_data
{
	char topic[17];
	char msg[17];
};

int check_if_subscribed(const char *name, const char *client_ip)
{
	//tutaj użyto fopen itd. zamiast posixowych ze względu na możliwość
	//odczytu pliku linijka po linijce
	FILE *fp;
	char *line = NULL;
	size_t len = 0;

	fp = fopen(name, "r");
	//blokada pliku
	while(flock(fileno(fp), LOCK_EX) != 0);

	while(getline(&line, &len, fp) != -1)
	{
		if(strncmp(client_ip, line, len - 1) == 0)
			return 1;
	}

	//zwolnienie blokady pliku
	flock(fileno(fp), LOCK_UN);
	fclose(fp);
	free(line);
	return 0;
}

void* handle_sub_request(void *data)
{
	int fd;
	struct sub_req_data *sub_data = (struct sub_req_data*)data;
	char is_subscribed = 0;
	char file_name[32];

	//sprawdzenie czy klient subskrubuje dany temat
	sprintf(file_name, "%s/%s", DIRECTORY, sub_data->topic);
	//jeśli plik istnieje to sprawdzamy czy klient subskrybuje temat
	if(access(file_name, F_OK) != 0)
	{
		is_subscribed = check_if_subscribed(file_name, inet_ntoa(sub_data->client.sin_addr));
	}
	else
	{
		//jeśli plik nie istnieje tworzymy plik
		fd = open(file_name, O_CREAT);
		close(fd);
	}

	//jeśli nie subskrybuje
	if(!is_subscribed)
	{
		fd = open(file_name, O_WRONLY | O_APPEND);
		flock(fd, LOCK_EX);
		write(fd, inet_ntoa(sub_data->client.sin_addr), 32);
		write(fd, "\n", 32);
		flock(fd, LOCK_UN);
		close(fd);
	}

	return NULL;
}

void* handle_pub_request(void *data)
{
	return NULL;
}

void handle_connection(int fd, struct sockaddr_in addr)
{
	pthread_t thread;
	char buf[34];
	char topic[17];
	char msg[17];

	memset(buf, '\0', 34);
	memset(topic, '\0', 17);
	memset(msg, '\0', 17);

	if(recv(fd, buf, 33, 0) == -1)
	{
		printf("Funkcja recv() zwróciła błąd\n");
		return;
	}

	strncpy(topic, &buf[1], 16);
	strncpy(msg, &buf[17], 16);

	if(buf[0] == 's')
	{
		struct sub_req_data *data;
		data = malloc(sizeof(struct sub_req_data));
		data->client = addr;
		strcpy(data->topic, topic);
		if(pthread_create(&thread, NULL, handle_sub_request, data) != 0)
			printf("Funkcja pthread_create() zwróciła błąd\n");
	}
	else if(buf[0] == 'p')
	{
		struct pub_req_data *data;
		data = malloc(sizeof(struct pub_req_data));
		strcpy(data->topic, topic);
		strcpy(data->msg, msg);
		if(pthread_create(&thread, NULL, handle_pub_request, data) != 0)
			printf("Funkcja pthread_create() zwróciła błąd\n");
	}
	else
		printf("Nieprawidłowe żądanie od klienta\n");

	close(fd);
	return;
}

int main()
{
	int sock_fd, in_fd;
	struct sockaddr_in addr;
	struct sockaddr_in in_addr;
	socklen_t sin_size;
	int yes = 1;
	struct stat st;

	//ścieżka w której będą przechowywane informacje o subskrybcji
	if(stat(DIRECTORY, &st) == -1)
	{
		mkdir(DIRECTORY, 0777);
	}
	else
	{
		printf("Nie można utworzyć ścieżki");
		return -1;
	}

	//połączenie
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
