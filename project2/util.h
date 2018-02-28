int WINDOW_SIZE = 5120;
const int MAX_PACKET_SIZE = 1024;

void error(char *msg) {
    perror(msg);
    exit(1);
}

enum Flag { RETRANSMISSION, SYN, FIN };
const int PAYLOAD_SIZE = MAX_PACKET_SIZE - 2 * sizeof(int) - sizeof(enum Flag);

struct Packet
{
    int sequenceNum;
    int ackNum;
    enum Flag flag;
    char payload[PAYLOAD_SIZE + 1];
};

struct Packet* initPacket(const char *data) {
    struct Packet *newPacket = malloc(sizeof(struct Packet));
    bzero(newPacket->payload, PAYLOAD_SIZE + 1);
    memcpy(newPacket->payload, data, PAYLOAD_SIZE);
    newPacket->payload[PAYLOAD_SIZE] = '\0';
    return newPacket;
}

void destructPacket(struct Packet *packet) {
    free(packet);
}