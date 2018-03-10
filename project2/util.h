#ifndef UTIL_H
#define UTIL_H

#include <sys/time.h>

static const struct timeval TIMEOUT = {0, 500000};
static const struct timeval CONNECTION_TIMEOUT = {1, 0};
#define WINDOW_SIZE 5120
#define MAX_PACKET_SIZE 1024
#define MAX_SEQUENCE_NUMBER 30720

void error(char *msg);

enum Flag { NONE, RETRANSMISSION, SYN, FIN, SYN_ACK, ACK };
// Leave 1 char for NULL termination
#define PAYLOAD_SIZE (MAX_PACKET_SIZE - 2 * sizeof(int) - sizeof(enum Flag) + 1)

struct Packet
{
    int offset;    
    int ackNum;
    enum Flag flag;
    char payload[PAYLOAD_SIZE];
    int received;
};

struct Packet* initPacket(const char *data, int offset, int ackNum, enum Flag flag);

void destructPacket(struct Packet *packet);

void freeResponse(struct Packet **packets, int numPackets);

// Encode packet to C string structured:
//     offset | ackNum | flag | payload
void packetToBytes(const struct Packet *packet, char *byteArray);

void bytesToPacket(struct Packet *packet, const char *byteArray);

int getSequenceNumber(const int offset);

#endif