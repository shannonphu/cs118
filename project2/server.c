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
struct Packet** getPacketsResponse(const char *fileName);
void writePacketSocket(const int socket, struct sockaddr_in* socketAddress, socklen_t socketLength, const char *data);
void writeErrorToSocket(const int socket, struct sockaddr_in* socketAddress, socklen_t socketLength);
int getNumberPacketsForSize(long size);
long getFileSize(const char *fileName);


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
    while (1) {
        // receive a UDP datagram from a client
        bzero(buffer, WINDOW_SIZE);
        int n = recvfrom(sockfd, buffer, WINDOW_SIZE, 0, (struct sockaddr *) &cli_addr, &clilen);
        if (n < 0)
            error("ERROR in recvfrom");

        struct Packet **response = getPacketsResponse(buffer);
        if (response == NULL) {
            writeErrorToSocket(sockfd, &cli_addr, clilen);
        } else {
            // Send each packet to client
            long fsize = getFileSize(buffer);
            int numPackets = getNumberPacketsForSize(fsize);

            for (int i = 0; i < numPackets; i++) {
                struct Packet *packetPtr = response[i];

                // Make packet to write
                char packet[MAX_PACKET_SIZE];
                bzero(packet, MAX_PACKET_SIZE);

                packetToBytes(packetPtr, packet);

                fprintf(stderr, "%s\n", packet + 12);

                writePacketSocket(sockfd, &cli_addr, clilen, packet);
                for (int i = 0; i < MAX_PACKET_SIZE; ++i) {
                    printf("%02x ", packet[i]);
                }
                printf("\n\n");
            }
        }
    }
    return 0;
}

void writeErrorToSocket(const int socket, 
                struct sockaddr_in* socketAddress, 
                socklen_t socketLength)
{
    writePacketSocket(socket, (struct sockaddr_in *)socketAddress, socketLength, "404: The requested file cannot be found or opened.");
}

// Writes MAX_PACKET_SIZE amount in bytes from data to socket.
void writePacketSocket(const int socket, 
                struct sockaddr_in* socketAddress, 
                socklen_t socketLength, 
                const char *data) 
{
    int n = sendto(socket, data, MAX_PACKET_SIZE, 0, (struct sockaddr *)socketAddress, socketLength);

    if (n < 0) {
        error("ERROR sending data to socket");
    }
}

struct Packet** getPacketsResponse(const char *fileName) {    
    if (checkCorrectFile(fileName) != 1) {
        return NULL;
    }

    // Make array of pointers to Packet structs which hold the header and 
    // payload of response packets
    long fsize = getFileSize(fileName);
    int numPackets = getNumberPacketsForSize(fsize);
    struct Packet **packets = malloc(numPackets * sizeof(struct Packet *));

    FILE *f = fopen(fileName, "rb");
    if (f != NULL) {
        // Read file into packets array
        char payloadTemp[PAYLOAD_SIZE + 1];
        for (int i = 0; i < numPackets; i++) {
            bzero(payloadTemp, PAYLOAD_SIZE + 1);
            fread(payloadTemp, 1, PAYLOAD_SIZE, f);
            packets[i] = initPacket(payloadTemp);
            // fprintf(stderr, "%s", packets[i]->payload);
        }
        fclose(f);
    }
    
    return packets;
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