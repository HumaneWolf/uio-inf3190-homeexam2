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
    if (argc < 4)
    { //Not enough args
        printf("Syntax: %s <MIP Transport Socket> <Target> <Port> <Filename>\n", argv[0]);
        return EXIT_SUCCESS;
    }

    // Args:
    char * sockpath = argv[1];
    unsigned char target = (char)atoi(argv[2]);
    unsigned short int port = atoi(argv[3]);
    char * path = argv[4];

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

    // Open file.
    FILE * f = fopen(path, "r");

    // Find end of file.
    if (fseek(f, 0L, SEEK_END) == -1) {
        perror("fseek: getting end of file.");
        exit(EXIT_FAILURE);
    }

    // Size, cast to int (we're not gonna send large files in this case).
    short int size = (short int)ftell(f);
    short int sendSize = htons(size);


    // Go back to start of file.
    if (fseek(f, 0L, SEEK_SET) == -1) {
        perror("fseek: getting start of file.");
        exit(EXIT_FAILURE);
    }

    // Get ready to send the file and the data.
    char * buffer = malloc(size + 5);

    buffer[0] = target;                 // Target
    memcpy(&(buffer[1]), &port, 2);     // Port
    memcpy(&(buffer[3]), &sendSize, 2); // Size

    // File
    fread(&(buffer[5]), 1, size, f);

    // Send it.
    if (send(sock, buffer, (size + 5), 0) == -1) {
        perror("send: sending file");
    }

    fclose(f);
    close(sock);
}