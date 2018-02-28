#ifndef UTIL_H
#define UTIL_H

static const int WINDOW_SIZE = 5120;
static const int MAX_PACKET_SIZE = 1024;

void error(char *msg);

enum Flag { RETRANSMISSION, SYN, FIN };
static const int PAYLOAD_SIZE = MAX_PACKET_SIZE - 2 * sizeof(int) - sizeof(enum Flag);

struct Packet
{
    int sequenceNum;
    int ackNum;
    enum Flag flag;
    char payload[PAYLOAD_SIZE + 1];
};

struct Packet* initPacket(const char *data);

void destructPacket(struct Packet *packet);

// Encode packet to C string structured:
//     sequenceNum | ackNum | flag | payload
void packetToBytes(const struct Packet *packet, char *byteArray);

void bytesToPacket(struct Packet *packet, const char *byteArray);

#endif