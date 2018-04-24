#include "ethernet.h"

#include <arpa/inet.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    if (argc < 2)
    { //Not enough args
        printf("Syntax: %s <MIP Transport Socket> <Port>\n", argv[0]);
        return EXIT_SUCCESS;
    }

    // Args:
    char *sockpath = argv[1];
    unsigned short int port = atoi(argv[2]);

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

    if (connect(sock, (struct sockaddr *)&sockaddr, sizeof(sockaddr)) == -1) {
        perror("connect()");
        exit(EXIT_FAILURE);
    }

    // Send port to listen at.
    if (send(sock, &port, 2, 0) == -1) {
        perror("send: failed to send port.");
        exit(EXIT_FAILURE);
    }

    // Variable
    int fileCounter = 0;
    char filename[20] = { 0 };

    while (1) {
        // Get size
        short int size = 0;
        if (recv(sock, &size, 2, 0) == -1) {
            perror("recv: failed to receive size");
            exit(EXIT_FAILURE);
        }
        size = ntohs(size);

        // Buffer
        char * buffer = malloc(size);
        int arrPointer = 0;

        // Start getting.
        while (arrPointer < (size - 1)) {
            int len = recv(sock, &(buffer[arrPointer]), (size - arrPointer), 0);
            if (len == -1) {
                perror("recv: failed to receive file data");
                exit(EXIT_FAILURE);
            }

            arrPointer = arrPointer + len;
        }

        // Save file.
        sprintf(filename, "file%d", fileCounter);
        FILE * f = fopen(filename, "w");

        fwrite(buffer, 1, size, f);

        fclose(f);
        fileCounter = fileCounter + 1;
    }

    close(sock);
}