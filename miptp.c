#include <string.h>


/**
 * Build a header and place it in the address pointed to by headerLoc.
 * Input:
 *      padding - The padding added to the packet. No more than 3.
 *      port - The port to send to and from.
 *      seqNum - The sequence number of the packet.
 *      headerLoc - The location to store the resulting header in.
 */
void buildHeader(unsigned char padding, unsigned short port, unsigned short seqNum, unsigned char * headerLoc) {
    unsigned int header = 0;

    // Padding
    header = (header & padding) << 14;

    // Port
    header = (header & htons(port)) << 16;

    // seqnum
    header = header & htons(seqNum);

    header = htonl(header);

    memcpy(headerLoc, &header, 4);
}


/**
 * Accepts the location of a header and returns the padding as an unsigned char.
 * Input:
 *      headerLoc - A pointer to the header.
 */
unsigned char getPadding(unsigned char * headerLoc) {
    unsigned int header = 0;
    memcpy(&header, headerLoc, 4);
    header = ntohl(header);

    return (header >> 30) & 3;
}


/**
 * Accepts the location of a header and returns the port as an unsigned short int.
 * Input:
 *      headerLoc - A pointer to the header.
 */
unsigned short getPort(unsigned char * headerLoc) {
    unsigned int header = 0;
    memcpy(&header, headerLoc, 4);
    header = ntohl(header);

    return (unsigned short)((header >> 16) & 0x3FFF);
}


/**
 * Accepts the location of a header and returns the port as an unsigned short int.
 * Input:
 *      headerLoc - A pointer to the header.
 */
unsigned short getSeqNum(unsigned char * headerLoc) {
    unsigned int header = 0;
    memcpy(&header, headerLoc, 4);
    header = ntohl(header);

    return (unsigned short)(header & 0xFFFF);
}
