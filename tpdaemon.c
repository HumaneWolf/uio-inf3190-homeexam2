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
struct miptp_record packetLog[10] = { 0 };

/**
 * Queue of packets to send.
 */
struct packet_linkedlist * sendingQueue = 0;

/**
 * List of incoming packets not yet sent to application.
 */
struct packet_linkedlist * receivedQueue = 0;

/**
 * List of connected applications and their ports.
 */
struct application_linkedlist * applicationList = 0;


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
        int sock = accept(epctrl->application_fd, &(epctrl->application_sockaddr), sizeof(epctrl->application_sockaddr));
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
        recv(app->socket, &(app->port), 2, 0); // The first 2 bytes sent by the connecting app should be the port.
    }
        // Received data from daemon
    else if (epctrl->events[n].data.fd == epctrl->daemon_fd)
    {
        
    }
        // Received new application.
    else if (epctrl->events[n].data.fd == epctrl->application_fd)
    {

    }
        // Received data from application.
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
    if (argc <= 2)
    {
        printf("Syntax: %s <daemon socket> <application socket>\n", argv[0]);
        return EXIT_SUCCESS;
    }

    // Variables
    char *daemon_sock_path = argv[1];
    char *app_sock_path = argv[2];

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

        // Handle timeouts.

        printf("Loop done.\n");
    }

    // Close unix socket.
    close(epctrl.daemon_fd);
    close(epctrl.application_fd);

    struct application_linkedlist * appTemp, * appTemp2;
    while (appTemp) { // Will be a pointer to 0 if undefined.
        close(appTemp->socket);
        appTemp2 = appTemp;
        free(appTemp);
    }

    return EXIT_SUCCESS;
}
