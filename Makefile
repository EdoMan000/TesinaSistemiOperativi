SERVER  = server
CLIENT  = client

SSRC    = server.c
CSRC    = client.c

all:
	gcc -o $(SERVER) $(SSRC) -pthread
	gcc -o $(CLIENT) $(CSRC) -pthread

