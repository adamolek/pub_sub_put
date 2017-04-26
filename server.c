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
#include <signal.h>

#define PORT 12345
#define CLIENT_PORT 23456
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
	struct sockaddr_in publisher;
};

void cleanup(int sig)
{
	char cmd[32];
	sprintf(cmd, "rm -rf %s", DIRECTORY);
	system(cmd);
	exit(0);
}

int check_if_subscribed(const char *name, const char *client_ip)
{
	//tutaj użyto fopen itd. zamiast posixowych ze względu na możliwość
	//odczytu pliku linijka po linijce
	FILE *fp;
	char *line = NULL;
	size_t len = 0;
	int ret = 0;

	fp = fopen(name, "r");
	//blokada pliku
	while(flock(fileno(fp), LOCK_EX) != 0);

	while(getline(&line, &len, fp) != -1)
	{
		if(len < 8)
			continue;
		if(strncmp(client_ip, line, strlen(client_ip)) == 0)
		{
			 ret = 1;
			 break;
		}
	}

	//zwolnienie blokady pliku
	flock(fileno(fp), LOCK_UN);
	fclose(fp);
	free(line);
	return ret;
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
	if(access(file_name, F_OK) != -1)
	{
		is_subscribed = check_if_subscribed(file_name, inet_ntoa(sub_data->client.sin_addr));
	}
	else
	{
		//jeśli plik nie istnieje tworzymy plik
		fd = open(file_name, O_CREAT, 0666);
		close(fd);
	}

	//jeśli nie subskrybuje
	if(!is_subscribed)
	{
		char buf[32];
		memset(buf, '\0', 32);
		sprintf(buf, "%s\n", inet_ntoa(sub_data->client.sin_addr));
		fd = open(file_name, O_WRONLY | O_APPEND);
		while(flock(fd, LOCK_EX) != 0);
		write(fd, buf, strlen(buf));
		flock(fd, LOCK_UN);
		close(fd);
	}

	return NULL;
}

int publish(const char *ip, const char *publisher_ip, const char *topic, const char *msg)
{
	int fd;
	char frame[50];
	struct sockaddr_in subscriber;

	fd = socket(AF_INET, SOCK_STREAM, 0);
	if(fd == -1)
		return -1;

	subscriber.sin_family = AF_INET;
	subscriber.sin_port = htons(CLIENT_PORT);
	inet_aton(ip, &subscriber.sin_addr);
	memset(&(subscriber.sin_zero), '\0', 8);

	if(connect(fd, (struct sockaddr*)&subscriber, sizeof(struct sockaddr)) == - 1)
		return -2;

	memset(frame, '\0', 50);
	strncpy(frame, publisher_ip, strlen(publisher_ip));
	frame[strlen(publisher_ip)] = ':';
	strncpy(frame + strlen(publisher_ip) + 1, topic, strlen(topic));
	frame[strlen(topic) + strlen(publisher_ip) + 1] = ':';
	strncpy(frame + strlen(topic) + strlen(publisher_ip) + 2, msg, strlen(msg));
	send(fd, frame, 50, 0);
	close(fd);

	return 0;
}

void* handle_pub_request(void *data)
{
	struct pub_req_data *pub_data = (struct pub_req_data*)data;
	FILE *fp;
	char *line = NULL;
	size_t len = 0;
	char file_name[32];
	char *publisher_ip = inet_ntoa(pub_data->publisher.sin_addr);

	sprintf(file_name, "%s/%s", DIRECTORY, pub_data->topic);
	fp = fopen(file_name, "r");
	//blokada pliku
	while(flock(fileno(fp), LOCK_EX) != 0);

	while(getline(&line, &len, fp) != -1)
	{
		if(len > 7)
			if(publish(line, publisher_ip, pub_data->topic, pub_data->msg) != 0)
				printf("Publikacja nie powiodła się dla adresu %s", line);
	}

	//zwolnienie blokady pliku
	flock(fileno(fp), LOCK_UN);
	fclose(fp);
	free(line);
	return NULL;
}

void* handle_unsub_request(void *data)
{
	struct sub_req_data *unsub_data = (struct sub_req_data*)data;
	FILE *fp_in, *fp_out;
	char *line = NULL;
	size_t len = 0;
	char file_name_in[32], file_name_out[32];
	char *subscriber_ip = inet_ntoa(unsub_data->client.sin_addr);
	char cmd[128];

	sprintf(file_name_in, "%s/%s", DIRECTORY, unsub_data->topic);
	fp_in = fopen(file_name_in, "r");
	if(fp_in == NULL)
	{
		printf("Funkcja fopen() zwróciła błąd\n");
		return NULL;
	}

	sprintf(file_name_out, "%s/__%s__", DIRECTORY, unsub_data->topic);
	fp_out = fopen(file_name_out, "w+");
	if(fp_out == NULL)
	{
		printf("Funkcja fopen() zwróciła błąd\n");
		return NULL;
	}

	while(flock(fileno(fp_in), LOCK_EX) != 0);

	while(getline(&line, &len, fp_in) != -1)
	{
		if(strncmp(subscriber_ip, line, strlen(subscriber_ip)) != 0)
			fwrite(line, sizeof(char), strlen(line), fp_out);
	}

	flock(fileno(fp_in), LOCK_UN);
	fclose(fp_out);
	fclose(fp_in);
	sprintf(cmd, "mv %s %s", file_name_out, file_name_in);
	system(cmd);
	free(line);

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
		data->publisher = addr;
		if(pthread_create(&thread, NULL, handle_pub_request, data) != 0)
			printf("Funkcja pthread_create() zwróciła błąd\n");
	}
	else if(buf[0] == 'u')
	{
		struct sub_req_data *data;
		data = malloc(sizeof(struct sub_req_data));
		data->client = addr;
		strcpy(data->topic, topic);
		if(pthread_create(&thread, NULL, handle_unsub_request, data) != 0)
			printf("Funckja pthread_create() zwrócieła błąd\n");
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

	//funkcja sprzątająca po serwerze
	signal(SIGINT, cleanup);
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
