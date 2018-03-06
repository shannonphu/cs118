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
#include <strings.h>
#include <unistd.h>

#include "util.h"

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
    struct Packet *requestPacket = initPacket(filename, -1, -1, SYN);
    char request[MAX_PACKET_SIZE];
    bzero(request, MAX_PACKET_SIZE);
    packetToBytes(requestPacket, request);
    int n = sendto(sockfd, request, MAX_PACKET_SIZE, 0, (struct sockaddr *)&serv_addr, serverlen);
    if (n < 0) {
        error("ERROR writing to socket");
    }

    // Setup file to write response to
    // FILE *fp = fopen("received.data", "w");
	// if (fp == NULL) {
		// error("Error opening file for writing");
    // }
    
    // Setup select
    fd_set sockets;
    FD_ZERO(&sockets);
    FD_SET(sockfd, &sockets);

    // Loop waiting for full response
    char buffer[MAX_PACKET_SIZE + 1];  
    while (1) {
        int selectResult = select(sockfd + 1, &sockets, NULL, NULL, NULL);
        if (selectResult < 0) {
            error("Select error");
        }

        if (FD_ISSET(sockfd, &sockets)) {
            bzero(buffer, MAX_PACKET_SIZE + 1);

            n = recvfrom(sockfd, buffer, MAX_PACKET_SIZE, 0, &serv_addr, &serverlen);
            if (n < 0) {
                error("ERROR reading from socket");
            } 

            struct Packet packet;
            bytesToPacket(&packet, buffer);
            
            if (packet.flag == FIN) {
                // fclose(fp);
                close(sockfd);                
                break;
            }

            // n = fwrite(packet.payload, sizeof(char), PAYLOAD_SIZE, fp);
            // if (n < 0) {
                // error("Error writing to received.data");
            // }
            printf("%s\n", packet.payload);
        }
    }
    
    // fclose(fp);
    return 0;
}
