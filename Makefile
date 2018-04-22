CC = gcc
FLAGS = -Wall -Werror -std=gnu11

TRANSPORTFILES = tpdaemon.c miptp.c debug.c
SERVERFILES = transserver.c
CLIENTFILES = transclient.c

CLEANFILES = bin/transportdaemon bin/trans_server bin/trans_client

all: transportdaemon client server

transportdaemon: $(TRANSPORTFILES)
	$(CC) $(FLAGS) $(TRANSPORTFILES) -o bin/trans_daemon

server: $(SERVERFILES)
	$(CC) $(FLAGS) $(SERVERFILES) -o bin/trans_server

client: $(CLIENTFILES)
	$(CC) $(FLAGS) $(CLIENTFILES) -o bin/trans_client

clean:
	rm -f $(CLEANFILES)
