int WINDOW_SIZE = 5120;
int MAX_PACKET_SIZE = 1024;

void error(char *msg) {
    perror(msg);
    exit(1);
}