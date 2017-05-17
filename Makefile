CC=gcc
CFLAGS=-Wall -lpthread

all:
	$(CC) client.c -o client $(CFLAGS)
	$(CC) server.c -o server $(CFLAGS)

clean:
	rm client server
