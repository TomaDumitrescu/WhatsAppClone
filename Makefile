CFLAGS = -Wall -g -Werror -Wno-error=unused-variable

# Listening port for server
PORT = 12345

# Server IP address
IP_SERVER = 192.168.0.2

all: server client

common.o: common.c

# Compile server.c
server: server.c -lpthread common.o

# Compile client.c
client: client.c common.o

.PHONY: clean run_server run_client

# Run the server
run_server:
	./server ${IP_SERVER} ${PORT}

# Run the client	
run_client:
	./client ${IP_SERVER} ${PORT}

clean:
	rm -rf server client *.o *.dSYM
