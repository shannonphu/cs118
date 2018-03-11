#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "util.h"

void error(char *msg) {
    perror(msg);
    exit(1);
}

void printPacket(char *buffer) {
    for (int i = 0; i < MAX_PACKET_SIZE; i++) {
        printf("%02x ", buffer[i]);
    }
    printf("\n\n");
}

struct Packet* initPacket(const char *data, int offset, int ackNum, enum Flag flag) {
    struct Packet *newPacket = malloc(sizeof(struct Packet));
    newPacket->offset = offset;
    newPacket->ackNum = ackNum;
    newPacket->flag = flag;
    bzero(newPacket->payload, PAYLOAD_SIZE);
    memcpy(newPacket->payload, data, PAYLOAD_SIZE);
    newPacket->received = 0;
    newPacket->sent = 0;
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
    memcpy(byteArray + sizeof(packet->offset) + sizeof(packet->ackNum) + sizeof(packet->flag), packet->payload, PAYLOAD_SIZE);
}

void bytesToPacket(struct Packet *packet, const char *byteArray) {
    memcpy(&(packet->offset), byteArray, sizeof(packet->offset));
    memcpy(&(packet->ackNum), byteArray + sizeof(packet->offset), sizeof(packet->ackNum));
    memcpy(&(packet->flag), byteArray + sizeof(packet->offset) + sizeof(packet->ackNum), sizeof(packet->flag));
    memcpy(packet->payload, byteArray + sizeof(packet->offset) + sizeof(packet->ackNum) + sizeof(packet->flag), PAYLOAD_SIZE);
}

int getSequenceNumber(const int offset) {
    return offset % (MAX_SEQUENCE_NUMBER + MAX_PACKET_SIZE);
}

void getFlagName(enum Flag flag, char *name) {
    switch (flag) {
        case SYN:
            strcpy(name, "SYN");
            break;
        case FIN:
            strcpy(name, "FIN");
            break;
        default:
            name = "";
            break;
    }
}

// Writes MAX_PACKET_SIZE amount in bytes from data to socket.
void writeDataToSocket(const int socket, struct sockaddr_in* socketAddress, socklen_t socketLength, const char *data) 
{
    int n = sendto(socket, data, MAX_PACKET_SIZE, 0, (struct sockaddr *)socketAddress, socketLength);

    if (n < 0) {
        error("ERROR sending data to socket");
    }
}

void writePacketToSocket(const int socket, struct sockaddr_in* socketAddress, socklen_t socketLength, const struct Packet *packet) {
    char buffer[MAX_PACKET_SIZE];
    bzero(buffer, MAX_PACKET_SIZE);
    packetToBytes(packet, buffer);
    writeDataToSocket(socket, socketAddress, socketLength, buffer);
}

void writeErrorToSocket(const int socket, struct sockaddr_in* socketAddress, socklen_t socketLength)
{
    writeDataToSocket(socket, (struct sockaddr_in *)socketAddress, socketLength, "404: The requested file cannot be found or opened.");
}