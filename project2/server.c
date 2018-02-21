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


void sigchld_handler(int s);
void error(char *msg);
int checkCorrectFile(const char *path);
int getResponse(const char *fileName, char *response);
void writeSocket(const int socket, struct sockaddr_in* socketAddress, socklen_t socketLength, const char *data);
void writeErrorToSocket(const int socket, struct sockaddr_in* socketAddress, socklen_t socketLength);


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
    
    int BUFFER_SIZE = 1024;
    char buffer[BUFFER_SIZE];    
    while (1) {
        // receive a UDP datagram from a client
        bzero(buffer, BUFFER_SIZE);
        int n = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *) &cli_addr, &clilen);
        if (n < 0)
            error("ERROR in recvfrom");

        char response[BUFFER_SIZE];
        bzero(response, BUFFER_SIZE);
        int status = getResponse(buffer, response);
        if (status < 0) {
            writeErrorToSocket(sockfd, &cli_addr, clilen);
        }

        // Write file contents to client
        writeSocket(sockfd, &cli_addr, clilen, response);
    }
    return 0;
}

void writeErrorToSocket(const int socket, 
                struct sockaddr_in* socketAddress, 
                socklen_t socketLength)
{
    writeSocket(socket, (struct sockaddr *)socketAddress, socketLength, "404: The requested file cannot be found or opened.");
}

void writeSocket(const int socket, 
                struct sockaddr_in* socketAddress, 
                socklen_t socketLength, 
                const char *data) 
{
    int n = sendto(socket, data, strlen(data), 0, (struct sockaddr *)socketAddress, socketLength);
    
    if (n < 0) {
        error("ERROR sending data to socket");
    }
}

int getResponse(const char *fileName, char *fileContents) {    
    if (checkCorrectFile(fileName) != 1) {
        return -1;
    }
    
    FILE *f = fopen(fileName, "rb");
    if (f != NULL) {
        fseek(f, 0, SEEK_END);
        long fsize = ftell(f);
        fseek(f, 0, SEEK_SET);

        bzero(fileContents, fsize);
        int numberBytes = fread(fileContents, 1, fsize, f);
        if (numberBytes != fsize)
            return -1;
        fileContents[fsize] = '\0';
        return 0;
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

void error(char *msg) {
    perror(msg);
    exit(1);
}

void sigchld_handler(int s) {
    while (waitpid(-1, NULL, WNOHANG) > 0);
}