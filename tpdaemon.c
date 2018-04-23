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
 * List of packets we need to send, but have not sent due to window size.
 */
struct packet_linkedlist * sendingQueue = 0;

/**
 * List of connected applications and their ports.
 */
struct application_linkedlist * applicationList = 0;

/**
 * List of incoming sequence numbers per port.
 */
unsigned short int inSeqNums[65535] = { 0 };

/**
 * List of outgoing sequence numbers per port.
 */
unsigned short int outSeqNums[65535] = { 0 };


/**
 * Add a file descriptor to the epoll.
 * Input:
 *      epctrl - Epoll controller struct.
 *      fd - The file descriptor to add.
 */
void epoll_add(struct epoll_control *epctrl, int fd)
{
    struct epoll_event ev = { 0 };
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
        unsigned char buffer[WINDOW_SIZE + 4];
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
                // If this packet is acked.
                if (packetLog[i].exists && packetLog[i].port == port && packetLog[i].seqnum <= seqNum)
                {
                    packetLog[i].exists = 0;
                    free(packetLog[i].data);

                    if (i < (WINDOW_SIZE - 1)) // If not the last (newest) element in the log.
                    {
                        // Move the queue one.
                        memcpy(&(packetLog[i]), &(packetLog[i + 1]), ((WINDOW_SIZE - i - 1) * sizeof(struct miptp_record)));
                        packetLog[WINDOW_SIZE - 1].exists = 0; // Mark last one as not in use, in case it was used.
                        i--; // Count the same number again.
                    }
                }
            }

            debug_print("Handled ack for %u on port %u.\n", seqNum, port);

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
        unsigned char outBuffer[5];
        outBuffer[0] = from;
        buildHeader(0, port, (seqNum + 1), &(outBuffer[1]));
        if (send(epctrl->events[n].data.fd, outBuffer, 5, 0)) {
            perror("epoll_event: send(ack)");
            return;
        }
        debug_print("Ack returned.\n");
    }
        // Received data from an application.
    else
    {
        // Find the sending application.
        struct application_linkedlist * app = applicationList;
        while (app)
        {
            if (app->socket == epctrl->events[n].data.fd)
            {
                break;
            }
            app = app->next;
        }
        if (!app) {
            return;
        }

        unsigned char to;
        if (recv(epctrl->events[n].data.fd, &to, 1, 0) == -1) {
            perror("epoll_event: recv(from app to)");
            return;
        }

        unsigned short int port;
        if (recv(epctrl->events[n].data.fd, &port, 2, 0) == -1) {
            perror("epoll_event: recv(from app port)");
            return;
        }

        // While there is data to load.
        while (1)
        {
            unsigned char buffer[MAX_PAYLOAD_SIZE];
            struct miptp_record packet = { 0 };

            int length = recv(epctrl->events[n].data.fd, buffer, MAX_PAYLOAD_SIZE, 0);
            if (length == -1) {
                perror("epoll_event: recv(from app)");
            }

            if (length == 0) {
                break;
            }

            for (packet.length = length; packet.length % 4 != 0; packet.length++);

            // Set values.
            packet.exists = 1;
            packet.addr = to;
            packet.port = port;
            outSeqNums[port]++;
            packet.seqnum = outSeqNums[port];
            packet.data = malloc(sizeof(packet.length + 5));
            
            // Set packet data.
            packet.data[0] = to;
            buildHeader((packet.length - length), port, outSeqNums[port], &(packet.data[1]));
            memcpy(&(packet.data[5]), buffer, length);

            // Add to account for MIPTP header and destination.
            packet.length = packet.length + 5;

            // Add to sending queue.
            struct packet_linkedlist * ll = malloc(sizeof(struct packet_linkedlist));
            ll->next = sendingQueue;
            ll->data = packet;
            sendingQueue = ll;

            // If we received less than we can receive.
            if (length != MAX_PAYLOAD_SIZE)
            {
                break;
            }
        }
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
        perror("main: connect()");
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
        perror("main: connect()");
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

        // Add packets from queue to sending log if possible.
        int i;
        for (i = 0; i < WINDOW_SIZE; i++) {
            // If the spot is free.
            if (!packetLog[i].exists)
            {
                // If no packet in queue.
                if (!sendingQueue)
                {
                    break;
                }

                // Find packet from queue.
                struct packet_linkedlist * toSend = sendingQueue, * newToSend = 0;
                while (1)
                {
                    if (toSend->next == 0)
                    {
                        if (newToSend != 0)
                        {
                            newToSend->next = 0; // previous packet's ref if prev queue packet exists.
                        } else {
                            sendingQueue = 0; // Otherwise clear queue reference.
                        }
                        break;
                    }
                    newToSend = toSend;
                    toSend = toSend->next;
                }

                // Store the data. Will be sent by resend script.
                packetLog[i] = toSend->data;

                free(toSend);
            }
        }

        // Handle timeouts and resends. Will also send unsent packets in the packet log.
        for (i = 0; i < WINDOW_SIZE; i++)
        {
            if (packetLog[i].exists && packetLog[i].last_sent < (time(NULL) - timeout))
            {
                if (send(epctrl.daemon_fd, packetLog[i].data, packetLog[i].length, 0) == -1) {
                    perror("main: send(resend)");
                }
                packetLog[i].last_sent = time(NULL);
                usleep(100);
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
