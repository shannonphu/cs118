#include <stdio.h>
#include <sys/types.h>   // definitions of a number of data types used in socket.h and netinet/in.h
#include <sys/socket.h>  // definitions of structures needed for sockets, e.g. sockaddr
#include <netinet/in.h>  // constants and structures needed for internet domain addresses, e.g. sockaddr_in
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>	/* for the waitpid() system call */
#include <signal.h>	/* signal name macros, and the kill() prototype */


void sigchld_handler(int s)
{
    while(waitpid(-1, NULL, WNOHANG) > 0);
}

void dostuff(int); /* function prototype */
void writeErrorResponse(int);
char* parseHTTP(char*);

void error(char *msg)
{
    perror(msg);
    exit(1);
}

int main(int argc, char *argv[])
{
     int sockfd, newsockfd, portno, pid;
     socklen_t clilen;
     struct sockaddr_in serv_addr, cli_addr;
     struct sigaction sa;          // for signal SIGCHLD

     if (argc < 2) {
         fprintf(stderr,"ERROR, no port provided\n");
         exit(1);
     }
     sockfd = socket(AF_INET, SOCK_STREAM, 0);
     if (sockfd < 0) 
        error("ERROR opening socket");
     bzero((char *) &serv_addr, sizeof(serv_addr));
     portno = atoi(argv[1]);
     serv_addr.sin_family = AF_INET;
     serv_addr.sin_addr.s_addr = INADDR_ANY;
     serv_addr.sin_port = htons(portno);
     
     if (bind(sockfd, (struct sockaddr *) &serv_addr,
              sizeof(serv_addr)) < 0) 
              error("ERROR on binding");
     
     listen(sockfd,5);
     
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
         newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
         
         if (newsockfd < 0) 
             error("ERROR on accept");
         
         pid = fork(); //create a new process
         if (pid < 0)
             error("ERROR on fork");
         
         if (pid == 0)  { // fork() returns a value of 0 to the child process
             close(sockfd);
             dostuff(newsockfd);
             exit(0);
         }
         else //returns the process ID of the child process to the parent
             close(newsockfd); // parent doesn't need this 
     } /* end of while */
     return 0; /* we never get here */
}

/******** DOSTUFF() *********************
 There is a separate instance of this function 
 for each connection.  It handles all communication
 once a connnection has been established.
 *****************************************/

void dostuff (int sock)
{
   int n;
   char buffer[256];
      
   bzero(buffer,256);
   n = read(sock,buffer,255);
   if (n < 0) error("ERROR reading from socket");
   
   // Print out the HTTP request
   // printf("HTTP Request Message:\n%s\n", buffer);

   char * fileName = parseHTTP(buffer);
   
   // Open file if it exists and write the contents to the HTTP response
   FILE *f = fopen(fileName, "rb");
   if (f != NULL) {
      fseek(f, 0, SEEK_END);
      long fsize = ftell(f);
      fseek(f, 0, SEEK_SET);
      
      char response[500000];
      bzero(response, 500000); 
      strcpy(response, "HTTP/1.1 200 OK\r\n\0");
      // write(sock, "HTTP/1.1 200 OK\r\n", 17);
      char contentLength[100];
      bzero(contentLength, 100); 
      sprintf(contentLength, "Content-Length: %ld\r\n", fsize);
      strcat(response, contentLength);
      // write(sock, metadata, strlen(metadata));

      // printf("File size: %ld\n", fsize);
 
      char *fileContents = (char *)malloc(sizeof(char) * (fsize));
      bzero(fileContents, sizeof(fileContents));
      int numberBytes = fread(fileContents, sizeof(char), fsize, f);
      printf("Read # of bytes from file: %d\n", numberBytes);
      printf("%x\n", fileContents);

      strcat(response, "Connection: keep-alive\r\n");
      // write(sock, "Connection: keep-alive\r\n", 24);
      // Start response body
      strcat(response, "\r\n");
      printf("Length of header: %d\n", strlen(response));
      // write(sock, "\r\n", 2);
      
      //strcat(response, fileContents);

      // printf("Bytes written: %d\n", n);
      // n = write(sock, fileContents, fsize);
      // printf("==========================================\n");
      printf("Response:\n%s\nTotal Response Length: %d\n\n\n", response, strlen(response));
      n = write(sock, response, strlen(response));
      printf("%d\n", n);
      if (n < 0) error("ERROR writing to socket");
      //write(sock, "Connection: close\r\n", 19);
      //write(sock, "\r\n", 2);
      
      write(sock, fileContents, fsize);

      // Cleanup
      free(fileContents);
      fclose(f);
   } else {
      writeErrorResponse(sock);
   }
}

char* parseHTTP(char* url) {
    //The string has to be at least 6 since we have GET / infront of it
    if (strlen(url) < 6)
        return NULL;

    char *temp = (char *) malloc(sizeof(char) * 5);
    temp[0] = url[0];
    temp[1] = url[1];
    temp[2] = url[2];
    temp[3] = url[3];
    temp[4] = url[4];

    if (strcmp(temp, "GET /") != 0)
	return NULL;

    int count = 5;
    while(url[count] != '\0' && url[count] != ' ') {
        count++;
    }
    char* path;
    if (count != 5) {
	path = (char *) malloc(sizeof(char) * (count - 2));
        int position = 5;
        path[0] = '.';
        path[1] = '/';
        while(url[position] != '\0' && url[position] != ' ') {
            path[position-3] = url[position];
            position++;
        }
        path[count - 2] = '\0';
    }
    else {
	//If there is no file, make file.html as a default file
        path = (char *) malloc(sizeof(char) * 13);
        path = "./file.html";
    }
    // printf("File: %s\n", path);
    return path;
}

const char *errorHTML = "<!DOCTYPE html><html><head><title>404 Not Found</title></head><body><h3>Error: 404 Not Found</h3><p>The requested page does not exist or ran into an error.</p></body></html>";
void writeErrorResponse(int sock) {
   write(sock, "HTTP/1.1 404 Not Found\r\n", 24);
   write(sock, "\r\n", 2);
   write(sock, errorHTML, strlen(errorHTML));
}
