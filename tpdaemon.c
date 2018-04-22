#include "debug.h"
#include "ethernet.h"
#include "miptp.h"
#include "tpdaemon.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <linux/if_packet.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <sys/types.h>
#include <sys/un.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


/**
 * Log of the window we have sent but has not been acked.
 */
struct miptp_record packetLog[WINDOW_SIZE] = { 0 };

/**
 * List of connected applications and their ports.
 */
struct application_linkedlist * applicationList = 0;

/**
 * List of incoming sequence numbers per port.
 */
unsigned short int inSeqNums[65535];

/**
 * List of outgoing sequence numbers per port.
 */
unsigned short int outSeqNums[65535];


/**
 * Add a file descriptor to the epoll.
 * Input:
 *      epctrl - Epoll controller struct.
 *      fd - The file descriptor to add.
 */
void epoll_add(struct epoll_control *epctrl, int fd)
{
    struct epoll_event ev = {0};
    ev.events = EPOLLIN;
    ev.data.fd = fd;
    if (epoll_ctl(epctrl->epoll_fd, EPOLL_CTL_ADD, fd, &ev) == -1)
    {
        perror("epoll_add: epoll_ctl()");
        exit(EXIT_FAILURE);
    }
}


/**
 * Handle an incoming connection event from any socket.
 * Input:
 *      epctrl - The epoll controller struct.
 *      n - The event counter, says which event to handle.
 */
void epoll_event(struct epoll_control *epctrl, int n)
{
        // Accept a new connection.
    if (epctrl->events[n].data.fd == epctrl->application_fd) {
        // Accept connection.
        int sock = accept(epctrl->application_fd, &(epctrl->application_sockaddr), &(epctrl->application_sockaddrlen));
        if (sock == -1) {
            perror("epoll_event: accept()");
            exit(EXIT_FAILURE);
        }

        // Add to epoll.
        epoll_add(epctrl, sock);

        // Store a ref in our list.
        struct application_linkedlist * app = malloc(sizeof(struct application_linkedlist));
        app->next = applicationList;
        app->socket = sock;

        // Get port.
        // The first 2 bytes immidately sent by the connecting app should be the port.
        // Only needed if server should accept incoming connections, otherwise app should send 0.
        if (recv(app->socket, &(app->port), 2, 0) == -1) {
            perror("epoll_event: recv(port)");
            return;
        }
        
        if (app->port)
        {
            debug_print("New application connected, listening on port %u.\n", app->port);
        } else {
            debug_print("New application connected.\n");
        }
    }
        // Received data from daemon
    else if (epctrl->events[n].data.fd == epctrl->daemon_fd)
    {
        unsigned char buffer[1496];
        unsigned char from;
        if (recv(epctrl->events[n].data.fd, &from, 1, 0) == -1) {
            perror("epoll_event: recv(daemon, from)");
            return;
        }

        int length = recv(epctrl->events[n].data.fd, &buffer, sizeof(buffer), 0);
        if (length == -1) {
            perror("epoll_event: recv(daemon)");
            return;
        }

        // Variables
        unsigned char padding = getPadding(buffer);
        unsigned short int port = getPort(buffer);
        unsigned short seqNum = getSeqNum(buffer);

        // Handle ack.
        if (length == 4)
        {
            int i;
            for (i = 0; i < WINDOW_SIZE; i++)
            {
                if (packetLog[i].exists && packetLog[i].port == port && packetLog[i].seqnum <= seqNum)
                {
                    packetLog[i].exists = 0;
                    free(packetLog[i].data);
                }
            }

            return; // Don't do any more if it's an ack.
        }

        // Check if sequence number fits.
        // TODO: Change sequence numbers to loop around.
        if (seqNum == (inSeqNums[port] + 1))
        {
            inSeqNums[port]++;
        } else {
            debug_print(
                "Packet received on port %u, but seqnum mismatch: expected %u, got %u.\n",
                port, inSeqNums[port], seqNum
            );
            return;
        }

        struct application_linkedlist * appTemp = applicationList;
        while (appTemp) {
            if (appTemp->port != 0 && port == appTemp->port)
            {
                break; // Found the right app.
            }

            appTemp = appTemp->next;
        }

        // Send the payload to the application.
        if (send(appTemp->socket, &(buffer[4]), (length - 4 - padding), 0) == -1) {
            perror("epoll_event: send(to app)");
        }

        debug_print("Payload received on port %u and sent to application.\n", port);

        // Send ack
        // TODO: Send after a delay, to ack multiple packets at once.
        unsigned char outBuffer[4];
        buildHeader(0, port, (seqNum + 1), outBuffer);
        if (send(epctrl->events[n].data.fd, outBuffer, 4, 0)) {
            perror("epoll_event: send(ack)");
            return;
        }
        debug_print("Ack returned.\n");
    }
        // Received data from an application.
    else
    {
        
    }
}


/**
 * Main method.
 */
int main(int argc, char *argv[])
{
    // Args count check
    if (argc <= 3)
    {
        printf("Syntax: %s [-d] <timeout in seconds> <daemon socket> <application socket>\n", argv[0]);
        return EXIT_SUCCESS;
    }

    int argBase = 1;
    if (!strcmp(argv[1], "-d"))
    {
        argBase++;
        enable_debug_print();
    }

    // Variables
    int timeout = atoi(argv[argBase]);
    char * daemon_sock_path = argv[argBase + 1];
    char * app_sock_path = argv[argBase + 2];

    // Create the daemon socket.
    int sock = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    if (sock == -1) {
        perror("main: socket()");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_un sockaddr;
    sockaddr.sun_family = AF_UNIX;
    strcpy(sockaddr.sun_path, daemon_sock_path);

    if (connect(sock, (struct sockaddr *)&sockaddr, sizeof(sockaddr)) == -1) {
        perror("main: connect(r)");
        exit(EXIT_FAILURE);
    }

    // Create epctrl struct
    struct epoll_control epctrl;
    epctrl.daemon_fd = sock;

    // Create the application socket.
    sock = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    if (sock == -1) {
        perror("main: socket()");
        exit(EXIT_FAILURE);
    }

    sockaddr.sun_family = AF_UNIX;
    strcpy(sockaddr.sun_path, app_sock_path);

    if (connect(sock, (struct sockaddr *)&sockaddr, sizeof(sockaddr)) == -1) {
        perror("main: connect(f)");
        exit(EXIT_FAILURE);
    }

    // Create EPOLL.
    epctrl.application_fd = sock;
    memcpy(&(epctrl.application_sockaddr), &sockaddr, sizeof(sockaddr));
    
    epctrl.epoll_fd = epoll_create(10);

    epoll_add(&epctrl, epctrl.application_fd);
    epoll_add(&epctrl, epctrl.daemon_fd);

    if (epctrl.epoll_fd == -1) {
        perror("main: epoll_create()");
        exit(EXIT_FAILURE);
    }

    printf("Ready to serve.\n");

    // Serve.
    while (1)
    {
        int nfds, n;
        nfds = epoll_wait(epctrl.epoll_fd, epctrl.events, MAX_EVENTS, 1000); // Max waiting time = 1 sec.
        if (nfds == -1) {
            perror("main: epoll_wait()");
            exit(EXIT_FAILURE);
        }

        for (n = 0; n < nfds; n++)
        {
            // Handle event.
            epoll_event(&epctrl, n);
        }

        // Handle timeouts and resends.
        int i;
        for (i = 0; i < WINDOW_SIZE; i++)
        {
            if (packetLog[i].exists && packetLog[i].last_sent < (time(NULL) - timeout))
            {
                if (send(epctrl.daemon_fd, packetLog[i].data, packetLog[i].length, 0) == -1) {
                    perror("main: send(resend)");
                }
            }
        }

        debug_print("Serve loop done.\n");
    }

    // Close unix socket.
    close(epctrl.daemon_fd);
    close(epctrl.application_fd);

    struct application_linkedlist * appTemp, * appTemp2;
    while (appTemp) // Will be a pointer to 0 if undefined.
    {
        close(appTemp->socket);
        appTemp2 = appTemp;
        free(appTemp);
        appTemp = appTemp2->next;
    }

    return EXIT_SUCCESS;
}
