#ifndef _miptp_h
#define _miptp_h

void buildHeader(unsigned char padding, unsigned short port, unsigned short seqNum, unsigned char * headerLoc);
unsigned char getPadding(unsigned char * headerLoc);
unsigned short getPort(unsigned char * headerLoc);
unsigned short getSeqNum(unsigned char * headerLoc);

#endif