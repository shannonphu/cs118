/*
 A simple client in the internet domain using TCP
 Usage: ./client hostname port (./client 192.168.0.151 10000)
 */
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdlib.h>
#include <strings.h>

int MAX_PACKET_SIZE = 1024;

void error(char *msg)
{
    perror(msg);
    exit(0);
}

int main(int argc, char *argv[])
{
    int sockfd;
    int portno, n;
    struct sockaddr_in serv_addr;
    struct hostent *server;

    char buffer[256];
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
    
    server = gethostbyname(argv[1]); //takes a string like "www.yahoo.com", and returns a struct hostent which contains information, as IP address, address type, the length of the addresses...
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);
    }
    
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET; //initialize server's address
    bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
    serv_addr.sin_port = htons(portno);
    
    if (connect(sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr)) < 0)
        error("ERROR connecting");
    
    n = write(sockfd, filename, MAX_PACKET_SIZE);
    if (n < 0) 
         error("ERROR writing to socket");
    
    bzero(buffer,256);
    n = read(sockfd, buffer, 255);
    if (n < 0) 
         error("ERROR reading from socket");
    printf("%s\n",buffer);
    
    close(sockfd);
    
    return 0;
}