#include "myftp.h"

struct message_s createMessage(unsigned char type, unsigned int length)
{
    struct message_s message;
    memset(&message, 0, sizeof(struct message_s));
    strcpy(message.protocol, "myftp");
    message.type = type;
    message.length = htonl(length);
    return message;
}

void createFile(char *filepath, int length, int fd) {
    int count;
    FILE *fp = fopen(filepath, "wb");
    char payload[RECV_BUFFER_SIZE];
    while (length > 0) {
        memset(payload, 0, RECV_BUFFER_SIZE);
        if ((count = recv(fd, payload, RECV_BUFFER_SIZE, 0)) < 0) {
            printf("receive file data Error: %s (Errno:%d)\n", strerror(errno), errno);
            exit(1);
        }
        fwrite(payload, sizeof(char), count, fp);
        length -= count;
    }
    printf("file created\n");
    fclose(fp);
}

void sendFile(FILE *fp, int fd) {
    int count;
    char payload[SEND_BUFFER_SIZE];
    while (!feof(fp)) {
        memset(payload, 0, SEND_BUFFER_SIZE);
        size_t bytes_read = fread(payload, sizeof(char), SEND_BUFFER_SIZE, fp);
        if ((count = send(fd, payload, bytes_read, 0)) < 0) {
            printf("send file data Error: %s (Errno:%d)\n", strerror(errno), errno);
            exit(1);
        }
    }
    printf("file sent\n");
}