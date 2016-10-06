CC=g++
CFLAGS=-Wall -pedantic -Wextra

all: client server

client: client.cpp
	$(CC) $(CFLAGS) client.cpp -o client 

server: server.cpp
	$(CC) $(CFLAGS) server.cpp -o server 

clean:
	rm -f client
	rm -f server
