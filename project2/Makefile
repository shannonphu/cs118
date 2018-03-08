CC=gcc
CFLAGS = -lm
SERVER_OBJ = server.c util.c
CLIENT_OBJ = client.c util.c

all: server client

server: $(SERVER_OBJ)
	$(CC) $^ -o $@ $(CFLAGS)

client: $(CLIENT_OBJ)
	$(CC) $^ -o $@ $(CFLAGS)

clean:
	$(RM) *.o server client