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
    newPacket->sequenceNum = 2;
    newPacket->ackNum = 1;
    newPacket->flag = SYN;
    bzero(newPacket->payload, PAYLOAD_SIZE + 1);
    memcpy(newPacket->payload, data, PAYLOAD_SIZE);
    newPacket->payload[PAYLOAD_SIZE] = '\0';
    return newPacket;
}

void destructPacket(struct Packet *packet) {
    free(packet);
}

// Encode packet to C string structured:
//     sequenceNum | ackNum | flag | payload
void packetToBytes(const struct Packet *packet, char *byteArray) {
    memcpy(byteArray, &(packet->sequenceNum), sizeof(packet->sequenceNum));
    memcpy(byteArray + sizeof(packet->sequenceNum), &(packet->ackNum), sizeof(packet->ackNum));
    memcpy(byteArray + sizeof(packet->sequenceNum) + sizeof(packet->ackNum), &(packet->flag), sizeof(packet->flag));
    memcpy(byteArray + sizeof(packet->sequenceNum) + sizeof(packet->ackNum) + sizeof(packet->flag), packet->payload, strlen(packet->payload));
    
    // fprintf(stderr, "seq %d\n", *byteArray);
    // fprintf(stderr, "ack %d\n", *(byteArray + 4));
    // fprintf(stderr, "flag %d\n", *(byteArray + 8));
    // fprintf(stderr, "%s\n", byteArray + 12);
}

void bytesToPacket(struct Packet *packet, const char *byteArray) {
    int sequenceNum = *byteArray;
    packet->sequenceNum = sequenceNum;
    int ackNum = *(byteArray + sizeof(packet->sequenceNum));
    packet->ackNum = ackNum;
    enum Flag flag = *(byteArray + sizeof(packet->sequenceNum) + sizeof(packet->ackNum));
    packet->flag = flag;
    strcpy(packet->payload, byteArray + sizeof(packet->sequenceNum) + sizeof(packet->ackNum) + sizeof(packet->flag));
}