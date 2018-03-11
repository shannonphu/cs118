#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <signal.h>
#include <math.h>

#include "util.h"


void sigchld_handler(int s);
int checkCorrectFile(const char *path);
struct Packet** getPacketsResponse(const char *fileName, int *numPackets);
int getNumberPacketsForSize(long size);
long getFileSize(const char *fileName);
void setPacketReceived(struct Packet **packets, int numPackets, int offset);


int main(int argc, char *argv[]) {
    int sockfd, newsockfd, portno, pid;
    socklen_t clilen;
    struct sockaddr_in serv_addr, cli_addr;
    struct sigaction sa;
    
    if (argc < 2) {
        fprintf(stderr, "ERROR, no port provided\n");
        exit(1);
    }

    // SOCK_DGRAM is for an unreliable, connectionless UDP protocol
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
        error("ERROR opening socket");

    /* setsockopt: Handy debugging trick that lets 
     * us rerun the server immediately after we kill it; 
     * otherwise we have to wait about 20 secs. 
     * Eliminates "ERROR on binding: Address already in use" error. 
    */
    int optval = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval, sizeof(int));
    // Set timeout of socket read
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&TIMEOUT, sizeof(TIMEOUT));

    bzero((char *)&serv_addr, sizeof(serv_addr));
    portno = atoi(argv[1]);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);
    
    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        error("ERROR on binding");
    
    listen(sockfd, 5);
    
    clilen = sizeof(cli_addr);
    
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }
    
    char buffer[WINDOW_SIZE];
    int windowLeftBound;
    int windowRightBound;
    int maxWindowRightBound;
    int numPackets;
    struct Packet **response = NULL;
    while (1) {
        // receive a UDP datagram from a client
        bzero(buffer, WINDOW_SIZE);
        int n = recvfrom(sockfd, buffer, WINDOW_SIZE, 0, (struct sockaddr *) &cli_addr, &clilen);
        // Set timeoutStatus to true if recvfrom has timeout error
        int timeoutStatus = n < 0;

        // Convert received packet from client into Packet struct
        struct Packet clientPacket;
        bytesToPacket(&clientPacket, buffer);

        // Received file request
        if (clientPacket.flag == SYN) {
            response = getPacketsResponse(clientPacket.payload, &numPackets);
            if (response == NULL) {
                writeErrorToSocket(sockfd, &cli_addr, clilen);
                continue;
            }

            // Reset congestion window bounds
            windowLeftBound = 0;
            windowRightBound = WINDOW_SIZE;
            maxWindowRightBound = numPackets * MAX_PACKET_SIZE;
        } else if (clientPacket.flag == ACK) {
            printf("Receiving packet %d\n", getSequenceNumber(clientPacket.ackNum));
            setPacketReceived(response, numPackets, clientPacket.ackNum);
            if (clientPacket.ackNum == windowLeftBound) {
                // If the leftmost item gets ACKed, move the window to 
                // the next unACKed packet. If all packets were just
                // ACKed in the window, select the first packet in the 
                // next window
                for (int i = windowLeftBound; i <= windowLeftBound + WINDOW_SIZE && i < maxWindowRightBound; i += MAX_PACKET_SIZE) {
                    int index = i / MAX_PACKET_SIZE;
                    struct Packet *packetPtr = response[index];

                    // Set the new window bounds starting at the next unreceived packet
                    if (packetPtr->received) {
                        windowLeftBound += MAX_PACKET_SIZE;
                        windowRightBound += MAX_PACKET_SIZE;
                    } else {
                        break;
                    }
                }
            }      
        } else if (clientPacket.flag == FIN_ACK) {
            printf("Receiving packet %d\n", getSequenceNumber(clientPacket.ackNum));

            // Free previous responses
            freeResponse(response, numPackets);
            numPackets = 0;
        }

        // Send each packet to client from window
        for (int i = windowLeftBound; i < windowLeftBound + WINDOW_SIZE && i < maxWindowRightBound && i / MAX_PACKET_SIZE < numPackets; i += MAX_PACKET_SIZE) {
            int index = i / MAX_PACKET_SIZE;
            struct Packet *packetPtr = response[index];

            // Don't resend packet if server received ACK or it is not timeout yet
            if (packetPtr->received || !timeoutStatus) {
                continue;
            }

            writePacketToSocket(sockfd, &cli_addr, clilen, packetPtr);
            char flagName[3] = {0};
            getFlagName(packetPtr->flag, flagName);
            printf("Sending packet %d %d %s\n", getSequenceNumber(packetPtr->offset), WINDOW_SIZE, flagName);
        }
    }
    return 0;
}

// Make array of pointers to Packet structs which hold the header and 
// payload of response packets; numPackets is passed in by reference to 
// return the number of packets in the response including a FIN packet
struct Packet** getPacketsResponse(const char *fileName, int *numPackets) {    
    long fsize = getFileSize(fileName);
    struct Packet **packets = NULL;

    if (fsize < 0 || checkCorrectFile(fileName) != 1) {
        // Default for error messages (error msg + FIN)        
        packets = malloc(2 * sizeof(struct Packet *));
        packets[0] = initPacket("404: The requested file cannot be found or opened.", 0, -1, NONE);
        packets[1] = initPacket("\0", MAX_PACKET_SIZE, -1, FIN);
        *numPackets = 2;
    } else {
        int packetCount = getNumberPacketsForSize(fsize);
        // Add one more packets on top of payload for FIN
        packets = malloc((packetCount + 1) * sizeof(struct Packet *));

        FILE *f = fopen(fileName, "rb");

        // Read file into packets array
        char payloadTemp[PAYLOAD_SIZE];
        // Loop over file contents excluding FIN
        for (int i = 0; i < packetCount; i++) {
            bzero(payloadTemp, PAYLOAD_SIZE);
            fread(payloadTemp, 1, PAYLOAD_SIZE, f);
            packets[i] = initPacket(payloadTemp, i * MAX_PACKET_SIZE, -1, NONE);
        }
        // Init a FIN packet last in response
        packets[packetCount] = initPacket("\0", packetCount * MAX_PACKET_SIZE, -1, FIN);
        *numPackets = packetCount + 1; // includes FIN packet
        fclose(f);
    }

    return packets;
}

void setPacketReceived(struct Packet **packets, int numPackets, int offset) {
    if (packets == NULL) {
        return;
    }

    for (int i = 0; i < numPackets; i++) {
        struct Packet *packet = packets[i];
        if (packet->offset == offset) {
            packet->received = 1;
            return;
        }
    }
}

int getNumberPacketsForSize(long size) {
    return ceil(size / (float)PAYLOAD_SIZE);
}

long getFileSize(const char *fileName) {
    FILE *f = fopen(fileName, "rb");
     if (f != NULL) {
        fseek(f, 0, SEEK_END);
        long fsize = ftell(f);
        fseek(f, 0, SEEK_SET);
        fclose(f);

        return fsize;
     } else {
         return -1;
     }
}

//Check if the user input a correct file format
int checkCorrectFile(const char *path) {
    struct stat st;
    
    if (stat(path, &st) < 0)
        return -1;
    
    return S_ISREG(st.st_mode);
}

void sigchld_handler(int s) {
    while (waitpid(-1, NULL, WNOHANG) > 0);
}