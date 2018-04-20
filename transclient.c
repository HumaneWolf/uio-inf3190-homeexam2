#include "ethernet.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    if (argc < 4)
    { //Not enough args
        printf("Syntax: %s <MIP Transport Socket> <Target> <Port> <Filename>\n", argv[0]);
        return EXIT_SUCCESS;
    }

    // Args:
    char *sockpath = argv[1];
    unsigned char target = (char)atoi(argv[2]);
    unsigned short int port = atoi(argv[3]);
    char *path = argv[4];

    // Create socket.
    int sock = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    if (sock == -1)
    {
        perror("socket()");
        exit(EXIT_FAILURE);
    }

    // Connect it.
    struct sockaddr_un sockaddr;
    sockaddr.sun_family = AF_UNIX;
    strcpy(sockaddr.sun_path, sockpath);

    if (connect(sock, (struct sockaddr *)&sockaddr, sizeof(sockaddr)) == -1)
    {
        perror("connect()");
        exit(EXIT_FAILURE);
    }

    // DO stuff.

    close(sock);
}