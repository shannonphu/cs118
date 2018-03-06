#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "util.h"

void error(char *msg) {
    perror(msg);
    exit(1);
}

struct Packet* initPacket(const char *data, int sequenceNum, int ackNum, enum Flag flag) {
    struct Packet *newPacket = malloc(sizeof(struct Packet));
    newPacket->sequenceNum = sequenceNum;
    newPacket->ackNum = ackNum;
    newPacket->flag = flag;
    bzero(newPacket->payload, PAYLOAD_SIZE + 1);
    memcpy(newPacket->payload, data, PAYLOAD_SIZE);
    newPacket->payload[PAYLOAD_SIZE] = '\0';
    return newPacket;
}

void destructPacket(struct Packet *packet) {
    free(packet);
}

void packetToBytes(const struct Packet *packet, char *byteArray) {
    memcpy(byteArray, &(packet->sequenceNum), sizeof(packet->sequenceNum));
    memcpy(byteArray + sizeof(packet->sequenceNum), &(packet->ackNum), sizeof(packet->ackNum));
    memcpy(byteArray + sizeof(packet->sequenceNum) + sizeof(packet->ackNum), &(packet->flag), sizeof(packet->flag));
    memcpy(byteArray + sizeof(packet->sequenceNum) + sizeof(packet->ackNum) + sizeof(packet->flag), packet->payload, strlen(packet->payload));
}

void bytesToPacket(struct Packet *packet, const char *byteArray) {
    int sequenceNum = *byteArray;
    packet->sequenceNum = sequenceNum;
    int ackNum = *(byteArray + sizeof(packet->sequenceNum));
    packet->ackNum = ackNum;
    enum Flag flag = *(byteArray + sizeof(packet->sequenceNum) + sizeof(packet->ackNum));
    packet->flag = flag;
    strcpy(packet->payload, byteArray + sizeof(packet->sequenceNum) + sizeof(packet->ackNum) + sizeof(packet->flag));
}