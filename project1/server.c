#include <stdio.h>
#include <sys/types.h> // definitions of a number of data types used in socket.h and netinet/in.h
#include <sys/socket.h> // definitions of structures needed for sockets, e.g. sockaddr
#include <netinet/in.h> // constants and structures needed for internet domain addresses, e.g. sockaddr_in
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h> /* for the waitpid() system call */
#include <signal.h>   /* signal name macros, and the kill() prototype */

void sigchld_handler(int s) {
  while (waitpid(-1, NULL, WNOHANG) > 0)
    ;
}

int checkCorrectFile(const char *path);
void dostuff(int); /* function prototype */
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
  struct sigaction sa; // for signal SIGCHLD

  if (argc < 2) {
    fprintf(stderr, "ERROR, no port provided\n");
    exit(1);
  }
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0)
    error("ERROR opening socket");
  bzero((char *)&serv_addr, sizeof(serv_addr));
  portno = atoi(argv[1]);
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port = htons(portno);

  if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    error("ERROR on binding");

  listen(sockfd, 5);

  clilen = sizeof(cli_addr);

  /****** Kill Zombie Processes ******/
  sa.sa_handler = sigchld_handler; // reap all dead processes
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;
  if (sigaction(SIGCHLD, &sa, NULL) == -1) {
    perror("sigaction");
    exit(1);
  }
  /*********************************/

  while (1) {
    newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);

    if (newsockfd < 0)
      error("ERROR on accept");

    pid = fork(); // create a new process
    if (pid < 0)
      error("ERROR on fork");

    if (pid == 0) { // fork() returns a value of 0 to the child process
      close(sockfd);
      dostuff(newsockfd);
      exit(0);
    } else // returns the process ID of the child process to the parent
      close(newsockfd); // parent doesn't need this
  }                     /* end of while */
  return 0;             /* we never get here */
}

/******** DOSTUFF() *********************
 There is a separate instance of this function
 for each connection.  It handles all communication
 once a connnection has been established.
 *****************************************/

void dostuff(int sock) {
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
    // NEED TO SEND A RESPOND TO THE CLIENT
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
    bzero(fileContents, sizeof(fileContents));
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

const char *errorHTML = "<!DOCTYPE html><html><head><title>404 Not "
                        "Found</title></head><body><h3>Error: 404 Not "
                        "Found</h3><p>The requested page does not exist or ran "
                        "into an error.</p></body></html>";
void writeErrorResponse(int sock) {
  char errorResponse[500];
  sprintf(errorResponse, "HTTP/1.1 404 Not Found\r\nContent-Type: "
                         "text/html\r\nContent-Length: %d\r\n\r\n%s\0",
          strlen(errorHTML), errorHTML);
  write(sock, errorResponse, strlen(errorResponse));
}
