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

void sigchld_handler(int s) {
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

int checkCorrectFile(const char *path);
void execution(int);
void writeErrorResponse(int);
char *parseHTTPRequest(char *);

void error(char *msg) {
    perror(msg);
    exit(1);
}

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
    
    char buf[1024];
    int BUFSIZE = 1024;
    while (1) {
        // receive a UDP datagram from a client
        bzero(buf, BUFSIZE);
        int n = recvfrom(sockfd, buf, BUFSIZE, 0, (struct sockaddr *) &cli_addr, &clilen);
        if (n < 0)
            error("ERROR in recvfrom");

        printf("server received %d/%d bytes: %s\n", strlen(buf), n, buf);

        // echo the input back to the client 
        n = sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr *) &cli_addr, clilen);
        if (n < 0) 
            error("ERROR in sendto");
    }
    return 0;
}

void execution(int sock) {
    int n;
    char buffer[2048];
    
    bzero(buffer, 2048);
    n = read(sock, buffer, 2048);
    if (n < 0)
        error("ERROR reading from socket");
    
    // Print out the HTTP request
    printf("HTTP Request Message:\n%s\n", buffer);
    
    char *fileName = parseHTTPRequest(buffer);
    
    if (checkCorrectFile(fileName) != 1) {
        writeErrorResponse(sock);
        return;
    }
    
    // Open file if it exists and write the contents to the HTTP response
    FILE *f = fopen(fileName, "rb");
    if (f != NULL) {
        char metadata[600];
        bzero(metadata, 600);
        
        fseek(f, 0, SEEK_END);
        long fsize = ftell(f);
        fseek(f, 0, SEEK_SET);
        
        write(sock, "HTTP/1.1 200 OK\r\n", 17);
        sprintf(metadata, "Content-Length: %ld\r\n", fsize);
        write(sock, metadata, strlen(metadata));
        
        char *fileContents = (char *)malloc(sizeof(char) * (fsize + 1));
        bzero(fileContents, strlen(fileContents));
        int numberBytes = fread(fileContents, 1, fsize, f);
        
        write(sock, "Connection: keep-alive\r\n", 24);
        
        // Content-Type
        char *contentType;
        if (strrchr(fileName, '.') != NULL) {
            char *fileExtension = strrchr(fileName, '.');
            if (strcasecmp(fileExtension, ".jpeg") == 0 ||
                strcasecmp(fileExtension, ".jpg") == 0) {
                contentType = "Content-Type: image/jpeg\r\n\0";
            } else if (strcasecmp(fileExtension, ".gif") == 0) {
                contentType = "Content-Type: image/gif\r\n\0";
            } else if (strcasecmp(fileExtension, ".html") == 0 ||
                       strcasecmp(fileExtension, ".htm") == 0) {
                contentType = "Content-Type: text/html\r\n\0";
            }
            
            write(sock, contentType, strlen(contentType));
        }
        
        write(sock, "\r\n", 2);
        n = write(sock, fileContents, fsize);
        if (n < 0)
            error("ERROR writing to socket");
        write(sock, "Connection: close\r\n", 19);
        write(sock, "\r\n", 2);
        free(fileContents);
    } else {
        writeErrorResponse(sock);
    }
    
    free(fileName);
    fclose(f);
}

//Check if the user input a correct file format
int checkCorrectFile(const char *path) {
    struct stat st;
    
    if (stat(path, &st) < 0)
        return -1;
    
    return S_ISREG(st.st_mode);
}

// We want input to be of the format
// GET /test%20file.html HTTP/1.1\n.....
char *parseHTTPRequest(char *httpRequest) {
    // Look for the beginning of the file path
    char *requestPtr = strstr(httpRequest, "GET ");
    // Not a proper GET HTTP request
    if (requestPtr == NULL) {
        return NULL;
    }
    
    // Point at the first character of the file path
    char *pathBegin = requestPtr + 5;
    
    // Look for the end of the file path
    requestPtr = strstr(pathBegin, " HTTP/1.1\r\n");
    // Not a proper GET HTTP request
    if (requestPtr == NULL) {
        return NULL;
    }
    
    // Get index of space after file path
    int pathLength = requestPtr - pathBegin;
    
    // Parse out just the path name
    char *path = malloc(pathLength + 1);
    memcpy(path, pathBegin, pathLength);
    path[pathLength] = '\0';
    
    // Handle %20 spaces encoded in request
    requestPtr = strstr(path, "%20");
    while (requestPtr != NULL) {
        char filteredPath[pathLength];
        bzero(filteredPath, pathLength);
        
        // Copy text before %20
        memcpy(filteredPath, path, requestPtr - path);
        
        // Add space to replace the %20
        strcat(filteredPath, " ");
        
        // Concatenate the rest of the string to path name
        strcat(filteredPath, requestPtr + 3);
        
        // Reset path to updated version of file name to continue the loop
        strcpy(path, filteredPath);
        requestPtr = strstr(path, "%20");
        pathLength = strlen(path);
    }
    
    return path;
}

//request response
const char *errorHTML = "<!DOCTYPE html><html><head><title>404 Not "
"Found</title></head><body><h3>Error: 404 Not "
"Found</h3><p>The requested page does not exist or ran "
"into an error.</p></body></html>";
void writeErrorResponse(int sock) {
    char errorResponse[500];
    sprintf(errorResponse, "HTTP/1.1 404 Not Found\r\nContent-Type: "
            "text/html\r\nContent-Length: %lu\r\n\r\n%s",
            strlen(errorHTML), errorHTML);
    write(sock, errorResponse, strlen(errorResponse));
}
