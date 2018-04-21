CC = gcc
FLAGS = -Wall -Werror -std=gnu11

TRANSPORTFILES = tpdaemon.c miptp.c
CLIENTFILES = transclient.c
SERVERFILES = transserver.c

CLEANFILES = bin/transportdaemon bin/trans_server bin/trans_client

all: client server transportdaemon

transportdaemon: $(TRANSPORTFILES)
	$(CC) $(FLAGS) $(TRANSPORTFILES) -o bin/trans_daemon

server: $(SERVERFILES)
	$(CC) $(FLAGS) $(SERVERFILES) -o bin/trans_server

client: $(CLIENTFILES)
	$(CC) $(FLAGS) $(CLIENTFILES) -o bin/trans_client

clean:
	rm -f $(CLEANFILES)
