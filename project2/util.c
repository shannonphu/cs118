#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "util.h"

void error(char *msg) {
    perror(msg);
    exit(1);
}

struct Packet* initPacket(const char *data, int offset, int ackNum, enum Flag flag) {
    struct Packet *newPacket = malloc(sizeof(struct Packet));
    newPacket->offset = offset;
    newPacket->ackNum = ackNum;
    newPacket->flag = flag;
    bzero(newPacket->payload, PAYLOAD_SIZE + 1);
    memcpy(newPacket->payload, data, PAYLOAD_SIZE);
    newPacket->payload[PAYLOAD_SIZE] = '\0';
    newPacket->received = 0;
    return newPacket;
}

void destructPacket(struct Packet *packet) {
    free(packet);
}

void freeResponse(struct Packet **packets, int numPackets) {
    if (packets == NULL) {
        return;
    }

    for (int i = 0; i < numPackets; i++) {
        struct Packet *packet = packets[i];
        destructPacket(packet);
    }

    free(packets);
    packets = NULL;
}

void packetToBytes(const struct Packet *packet, char *byteArray) {
    memcpy(byteArray, &(packet->offset), sizeof(packet->offset));
    memcpy(byteArray + sizeof(packet->offset), &(packet->ackNum), sizeof(packet->ackNum));
    memcpy(byteArray + sizeof(packet->offset) + sizeof(packet->ackNum), &(packet->flag), sizeof(packet->flag));
    memcpy(byteArray + sizeof(packet->offset) + sizeof(packet->ackNum) + sizeof(packet->flag), packet->payload, strlen(packet->payload));
}

void bytesToPacket(struct Packet *packet, const char *byteArray) {
    memcpy(&(packet->offset), byteArray, sizeof(packet->offset));
    memcpy(&(packet->ackNum), byteArray + sizeof(packet->offset), sizeof(packet->ackNum));
    memcpy(&(packet->flag), byteArray + sizeof(packet->offset) + sizeof(packet->ackNum), sizeof(packet->flag));
    strcpy(packet->payload, byteArray + sizeof(packet->offset) + sizeof(packet->ackNum) + sizeof(packet->flag));
}

int getSequenceNumber(const int offset) {
    return offset % (MAX_SEQUENCE_NUMBER + MAX_PACKET_SIZE);
}