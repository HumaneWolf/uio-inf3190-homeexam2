#ifndef _router_h
#define _router_h

#include <time.h>
#include <sys/epoll.h>
#include <sys/socket.h>


#define MAX_EVENTS 20
/**
 * EPOLL control structure.
 */
struct epoll_control
{
    int epoll_fd;             // The file descriptor for the epoll.
    int daemon_fd;            // The file descriptor for the socket used to communicate with the mip daemon.
    int application_fd;       // The file descriptor for the soocket used to communicate with applications.
    struct epoll_event events[MAX_EVENTS];
};


/**
 * MIPTP header
 */
struct miptp_header
{
    unsigned short int port;        // Port part of header First 2 bits are padding length.
    unsigned short int seqnum;      // Sequence number part of header.
} __attribute__((packed));


/**
 * A record struct holding data for a single record, used to resend packages etc.
 */
struct miptp_record
{
    char awaiting_ack;                  // Whether it is waiting to be acked (just for reference marking if the slot is available or not).
    unsigned short int port;            // Port number to send to/from.
    unsigned char target;               // The intended recipient of the data.
    unsigned short int seqnum;          // The sequence number of the packet.
    unsigned int length;                // Length of the packet data, in bytes.
    char * full_packet;                 // Pointer to the full packet data, ready to send.
    time_t last_sent;                   // The time the packet was sent.
};


/**
 * A linked list struct used to store packets with an unknown length list (i.e. sending queue).
 */
struct packet_linkedlist
{
    struct packet_linkedlist * next;    // Next element in the list.
    struct miptp_record data;           // The miptp_record struct.
};

#endif