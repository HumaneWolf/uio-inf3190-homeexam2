#ifndef _router_h
#define _router_h

#include <time.h>
#include <sys/epoll.h>
#include <sys/socket.h>


#define WINDOW_SIZE 10


#define MAX_EVENTS 20
/**
 * EPOLL control structure.
 */
struct epoll_control
{
    int epoll_fd;                           // The file descriptor for the epoll.
    int daemon_fd;                          // The file descriptor for the socket used to communicate with the mip daemon.
    int application_fd;                     // The file descriptor for the soocket used to communicate with applications.
    struct sockaddr application_sockaddr;   // Sockaddr for application.
    socklen_t application_sockaddrlen;      // Length of sockaddr struct.
    struct epoll_event events[MAX_EVENTS];
};


/**
 * A record struct holding data for a single record, used to resend packages etc.
 */
struct miptp_record
{
    char exists;                            // Just a marker to make if checks quicker.
    unsigned short int port;                // Port number to send to/from.
    unsigned char addr;                     // The intended recipient or sender of the data.
    unsigned short int seqnum;              // The sequence number of the packet.
    unsigned int length;                    // Length of the packet data, in bytes.
    char * data;                            // Pointer to the full packet data, ready to send OR to data to send to client.
    time_t last_sent;                       // The time the packet was sent.
};


/**
 * A linked list struct used to store packets in a list.
 */
struct packet_linkedlist
{
    struct packet_linkedlist * next;        // Next element in the list.
    struct miptp_record data;               // The miptp_record struct.
};


/**
 * A linked list struct to store a list of applications and their ports.
 */
struct application_linkedlist
{
    struct application_linkedlist * next;   // Next element in the list.
    int socket;                             // The socket.
    unsigned short port;                    // The port.
};

#endif