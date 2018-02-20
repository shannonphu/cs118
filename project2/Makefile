CC=gcc
CFLAGS=-I.
SERVER_OBJ = server.o
CLIENT_OBJ = client.o

all: server client

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

server: $(SERVER_OBJ)
	$(CC) -o $@ $^ $(CFLAGS)

client: $(CLIENT_OBJ)
	$(CC) -o $@ $^ $(CFLAGS)

clean:
	$(RM) *.o server client