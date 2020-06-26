#include "myftp.h"

void main_task(in_addr_t ip, unsigned short port, char *command, char *filename) {
    int fd, count;
    struct sockaddr_in addr;
    unsigned int addrlen = sizeof(struct sockaddr_in);
    struct message_s message;

    fd = socket(AF_INET, SOCK_STREAM, 0);

    if (fd == -1) {
        perror("socket()");
        exit(1);
    }

    memset(&addr, 0, sizeof(struct sockaddr_in));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = ip;
    addr.sin_port = htons(port);

    if (connect(fd, (struct sockaddr *) &addr, addrlen) == -1) {
        perror("connect()");
        exit(1);
    }

    if (!strcmp(command, "list")) {
        //send LIST_REQUEST
        message = createMessage(0xA1, sizeof(struct message_s));
        if ((count = send(fd, &message, sizeof(struct message_s), 0)) < 0) {
            printf("LIST_REQUEST Error: %s (Errno:%d)\n", strerror(errno), errno);
            exit(1);
        };

        //receive LIST_REPLY
        if ((count = recv(fd, &message, sizeof(struct message_s), 0)) < 0) {
            printf("LIST_REPLY Error: %s (Errno:%d)\n", strerror(errno), errno);
            exit(1);
        };
        message.length = ntohl(message.length);

        //receive filelist
        int length = message.length - sizeof(struct message_s);
        char filelist[length];
        if ((count = recv(fd, filelist, length, 0)) < 0) {
            printf("filelist receive Error: %s (Errno:%d)\n", strerror(errno), errno);
            exit(1);
        };
        printf("%s\n", filelist);
    }
    else if (!strcmp(command, "put")) {
        FILE *fp = fopen(filename, "rb");
        if (!fp) {
            printf("File Not Exist\n");
            exit(1);
        }
        fseek(fp, 0, SEEK_END);
        int filesize = ftell(fp);
        fseek(fp, 0, SEEK_SET);

        //send PUT_REQUEST
        message = createMessage(0xC1, sizeof(struct message_s) + strlen(filename) + 1);
        if ((count = send(fd, &message, sizeof(struct message_s), 0)) < 0) {
            printf("PUT_REQUEST Error: %s (Errno:%d)\n", strerror(errno), errno);
            exit(1);
        };
        if ((count = send(fd, filename, strlen(filename) + 1, 0)) < 0) {
            printf("filename error: %s (Errno:%d)\n", strerror(errno), errno);
            exit(1);
        };

        //receive PUT_REPLY
        if ((count = recv(fd, &message, sizeof(struct message_s), 0)) < 0) {
            printf("PUT_REPLY Error: %s (Errno:%d)\n", strerror(errno), errno);
            exit(1);
        }

        message = createMessage(0XFF, sizeof(struct message_s) + filesize);
        if ((count = send(fd, &message, sizeof(struct message_s), 0)) < 0) {
            printf("FILE_DATA Error: %s (Errno:%d)\n", strerror(errno), errno);
            exit(1);
        };
        sendFile(fp, fd);
        fclose(fp);
    }
    else if (!strcmp(command, "get")) {
        //Send GET_REQUEST
        message = createMessage(0xB1, sizeof(struct message_s) + strlen(filename) + 1);
        if ((count = send(fd, &message, sizeof(struct message_s), 0)) < 0) {
            printf("GET_REQUEST Error: %s (Errno:%d)\n", strerror(errno), errno);
            exit(1);
        }

        //Send filename
        if ((count = send(fd, filename, strlen(filename) + 1, 0)) < 0) {
            printf("filename send error: %s (Errno:%d)\n", strerror(errno), errno);
            exit(1);
        }

        //Receive GET_REPLY
        if ((count = recv(fd, &message, sizeof(struct message_s), 0)) < 0) {
            printf("GET_REPLY error: %s (Errno:%d)\n", strerror(errno), errno);
            exit(1);
        }
        message.length = ntohl(message.length);

        //File doesn't exist
        if (message.type == 0xB3) {
            printf("File Not Exist\n");
            exit(1);
        }
        else if (message.type == 0xB2) {
            //Receive FILE_DATA
            if ((count = recv(fd, &message, sizeof(struct message_s), 0)) < 0) {
                printf("Receive error: %s (Errno:%d)\n", strerror(errno), errno);
                exit(1);
            }
            message.length = ntohl(message.length);

            int length = message.length - sizeof(struct message_s);
            createFile(filename, length, fd);
        }
    }
    close(fd);
}

int main(int argc, char **argv) {
    in_addr_t ip;
    unsigned short port;
    char *command, *filename;

    if (argc < 4) {
        fprintf(stderr, "Usage: %s [IP address] [port] [list|get|put] [file]\n", argv[0]);
        exit(1);
    }

    if ((ip = inet_addr(argv[1])) == -1) {
        perror("inet_addr()");
        exit(1);
    }

    if (strcmp(argv[3], "list")
        && strcmp(argv[3], "get")
        && strcmp(argv[3], "put")) {
        fprintf(stderr, "Command not exist: %s\n", argv[3]);
        exit(1);
    }

    port = atoi(argv[2]);
    command = argv[3];
    if (!strcmp(command, "get") || !strcmp(command, "put"))
        filename = argv[4];
    else filename = NULL;

    main_task(ip, port, command, filename);
    return 0;
}
