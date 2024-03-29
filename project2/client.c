/*
 A simple client in the internet domain using TCP
 Usage: ./client hostname port filename (./client 192.168.0.151 10000 index.html)
 */
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "util.h"

void writeToFile(FILE *file, const char *data);
void printWindow(struct Packet *window, int count);

int main(int argc, char *argv[])
{
    int sockfd;
    int portno;
    struct sockaddr_in serv_addr;
    struct hostent *server;

    if (argc < 4) {
       fprintf(stderr,"usage ./%s hostname port filename\n", argv[0]);
       exit(0);
    }
    
    portno = atoi(argv[2]);
    char *filename = argv[3];

    // SOCK_DGRAM is for an unreliable, connectionless UDP protocol
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");

    server = gethostbyname(argv[1]);
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);
    }
    
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET; //initialize server's address
    bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
    serv_addr.sin_port = htons(portno);    
    socklen_t serverlen = sizeof(serv_addr);

    // Send file request packet
    struct Packet requestPacket;
    requestPacket.flag = SYN;
    memcpy(requestPacket.payload, filename, strlen(filename));
    writePacketToSocket(sockfd, &serv_addr, serverlen, &requestPacket);
    printf("Sending packet SYN\n");

    // Setup file to write response to
    FILE *fp = fopen("received.data", "w");
	if (fp == NULL) {
		error("Error opening file for writing");
    }
    
    // Setup select
    fd_set sockets;

    // Loop waiting for full response
    char buffer[MAX_PACKET_SIZE + 1];  
    struct Packet receiveWindow[WINDOW_SIZE / MAX_PACKET_SIZE] = {0};
    int receiveWindowBase = 0;
    int doneReceivingData = 0;
    int requestReceived = 0;
    while (1) {
        FD_ZERO(&sockets);
        FD_SET(sockfd, &sockets);

        struct timeval CONNECTION_TIMEOUT = {1, 0};
        int selectResult = select(sockfd + 1, &sockets, NULL, NULL, &CONNECTION_TIMEOUT);

        if (selectResult < 0) {
            fclose(fp);
            error("Select error");
        } else if (selectResult == 0) {
            if (doneReceivingData) {
                fclose(fp);
                return 1;
            }

            if (!requestReceived) {
                writePacketToSocket(sockfd, &serv_addr, serverlen, &requestPacket);
                printf("Sending packet Retransmission SYN\n");
            }
        }

        if (FD_ISSET(sockfd, &sockets)) {
            bzero(buffer, MAX_PACKET_SIZE + 1);

            int n = recvfrom(sockfd, buffer, MAX_PACKET_SIZE, 0, (struct sockaddr *)&serv_addr, &serverlen);
            if (n < 0) {
                error("ERROR reading from socket");
            } 

            requestReceived = 1;

            struct Packet packet;
            bzero(&packet, sizeof(struct Packet));
            bytesToPacket(&packet, buffer);
            printf("Receiving %d\n", getSequenceNumber(packet.offset));

            // Send an ACK for received packets with payload
            struct Packet ackPacket;

            switch(packet.flag) {
                case FIN:
                    ackPacket.flag = FIN_ACK;
                    // If still waiting for unreceived packets, ignore FIN to wait for more data, otherwise close the file
                    if (packet.offset != receiveWindowBase) {
                        continue;
                    }
                    doneReceivingData = 1;
                    break;
                default:
                    ackPacket.flag = ACK;
                    break;
            }
            ackPacket.offset = -1;
            ackPacket.ackNum = packet.offset;
            writePacketToSocket(sockfd, &serv_addr, serverlen, &ackPacket);

            char flagName[3] = {0};
            getFlagName(ackPacket.flag, flagName);

            // Don't add past received packets to the window or write to file
            if (packet.offset < receiveWindowBase) {
                printf("Sending packet %d Retransmission %s\n", getSequenceNumber(ackPacket.ackNum), flagName);
                continue;
            } else {
                printf("Sending packet %d %s\n", getSequenceNumber(ackPacket.ackNum), flagName);
            }

            if (packet.flag == FIN) {
                continue;
            }

            // Place the packet into the buffer in order within receive window
            for (int i = 0; i < WINDOW_SIZE / MAX_PACKET_SIZE; i++) {
                if (receiveWindowBase + i * MAX_PACKET_SIZE == packet.offset) {
                    bzero(&receiveWindow[i], sizeof(struct Packet));
                    receiveWindow[i] = packet;
                    receiveWindow[i].received = 1;
                    // printWindow(receiveWindow, WINDOW_SIZE / MAX_PACKET_SIZE);
                    break;
                }
            }

            // If received packet is at the beginning of the window,
            // write all following received data in window to file
            // and move remaining packets to be the new base
            if (receiveWindowBase == packet.offset) {
                int totalInWindow = 0;
                for (int j = 0; j < WINDOW_SIZE / MAX_PACKET_SIZE; j++) {
                    if (receiveWindow[j].received) {
                        // printf("Wrote packet %d to file\n", receiveWindow[j].offset);
                        char payload[PAYLOAD_SIZE + 1];
                        bzero(payload, PAYLOAD_SIZE + 1);
                        memcpy(&payload, receiveWindow[j].payload, PAYLOAD_SIZE);
                        writeToFile(fp, payload);
                        totalInWindow++;
                    } else {
                        // Move remaining packets to front of window
                        receiveWindowBase = receiveWindow[j - 1].offset + MAX_PACKET_SIZE;

                        int packetsToMove = (WINDOW_SIZE / MAX_PACKET_SIZE) - j; 
                        // Use sizeof Packets to include the received field in 
                        // the copied data since it isnt included in the 
                        // MAX_PACKET_SIZE
                        int bytesToMove = packetsToMove * sizeof(struct Packet); 
                        memcpy(receiveWindow, receiveWindow + j, bytesToMove);
                        bzero(receiveWindow + packetsToMove, ((WINDOW_SIZE / MAX_PACKET_SIZE) - packetsToMove) * sizeof(struct Packet));
                        // printWindow(receiveWindow, WINDOW_SIZE / MAX_PACKET_SIZE);
                        break;
                    }
                }

                // Window became full and all packets in the window were written to file
                if (totalInWindow == WINDOW_SIZE / MAX_PACKET_SIZE) {
                    receiveWindowBase += WINDOW_SIZE;
                    bzero(receiveWindow, sizeof(receiveWindow));
                }
            }

            // printf("\n");
        }
    }
    
    fclose(fp);
    return 0;
}

void writeToFile(FILE *file, const char *data) {
    int n = fprintf(file, "%s", data);
    if (n < 0) {
        error("Error writing to received.data");
    }
}

void printWindow(struct Packet *window, int count) {
    for (int i = 0; i < count; i++) {
        printf("%d,%d ", window[i].offset, window[i].received);
    }
    printf("\n");
}