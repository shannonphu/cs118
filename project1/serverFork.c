/* A simple server in the internet domain using TCP
   The port number is passed as an argument 
   This version runs forever, forking off a separate 
   process for each connection
*/
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
const char *errorHTML = "<!DOCTYPE HTML><html><head>404 Not Found</head><body><h3>Error: 404 Not Found</h3><p>The requested page does not exist or ran into an error.</p></body></html>\n";

void dostuff (int sock)
{
   int n;
   char buffer[256];
      
   bzero(buffer,256);
   n = read(sock,buffer,255);
   if (n < 0) error("ERROR reading from socket");
   // printf("Here is the message: %s\n",buffer);

   // TODO: Parse HTTP Request
   char * pch = (char *)strtok (buffer,"\n");
   while (pch != NULL) {
     // printf ("%s\n", pch);
     // Parse file name
     if (strstr(pch, "GET")) {
       printf ("%s\n", pch);
     }
     pch = (char *)strtok (NULL, "\n");
   }

   char * fileName = "error.html";
   
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
      write(sock, "\r\n", 2);
      n = write(sock, fileContents, fsize);
      if (n < 0) error("ERROR writing to socket");
      write(sock, "Connection: close\r\n", 19);
      write(sock, "\r\n", 2);
      free(fileContents);
      fclose(f);
   }
}
