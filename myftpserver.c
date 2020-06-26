#include "myftp.h"
#include <pthread.h>

char *getFilepath(char *filename) {
    int length = strlen(filename) + 6;
    char *filepath = (char *)malloc(length * sizeof(char));
    memset(filepath, 0, length);
    strcpy(filepath, "data/");
    strcat(filepath, filename);
    return filepath;
}

void *thr_func(void *arg) {
    pthread_detach(pthread_self());

    int fd = *(int *) arg;
    int count;
    struct message_s message;

    pthread_mutex_t lock;
    pthread_mutex_init(&lock, NULL);

    //receive message
    if ((count = recv(fd, &message, sizeof(struct message_s), 0)) < 0) {
        printf("Receive Error: %s (Errno:%d)\n", strerror(errno), errno);
        exit(1);
    }
    message.length = ntohl(message.length);

    //receive LIST_REQUEST
    if (message.type == 0xA1) {
        //lock this thread before getting filelist
        pthread_mutex_lock(&lock);

        //create filelist
        char payload[SEND_BUFFER_SIZE];
        memset(payload, 0, SEND_BUFFER_SIZE);
        DIR *dir = opendir("data");
        struct dirent *file;
        while (file = readdir(dir)) {
            if (strcmp(file->d_name, ".") && strcmp(file->d_name, "..")) {
                strcat(payload, file->d_name);
                strcat(payload, "\n");
            }
        }
        closedir(dir);

        //release lock as filelist is created
        pthread_mutex_unlock(&lock);

        //send LIST_REPLY
        message = createMessage(0xA2, sizeof(struct message_s) + strlen(payload) + 1);
        if ((count = send(fd, &message, sizeof(struct message_s), 0)) < 0) {
            printf("LIST_REPLY Error: %s (Errno:%d)\n", strerror(errno), errno);
            exit(1);
        }

        //send filelist
        //same concept, 1 is added to send the string
        if ((count = send(fd, payload, strlen(payload) + 1, 0)) < 0) {
            printf("filelist Error: %s (Errno:%d)\n", strerror(errno), errno);
            exit(1);
        }
    }
    //receive PUT_REQUEST
    else if (message.type == 0xC1) {
        //lock this thread before getting filedata
        pthread_mutex_lock(&lock);

        //receive filename
        char filename[message.length - sizeof(struct message_s)];
        if ((count = recv(fd, filename, sizeof(filename), 0)) < 0) {
            printf("filename receive error: %s (Errno:%d)\n", strerror(errno), errno);
            exit(0);
        }
        char *filepath = getFilepath(filename);

        //send PUT_REPLY
        message = createMessage(0xC2, sizeof(struct message_s));
        if ((count = send(fd, &message, sizeof(struct message_s), 0)) < 0) {
            printf("LIST_REPLY Error: %s (Errno:%d)\n", strerror(errno), errno);
            exit(1);
        };

        //receive FILE_DATA
        if ((count = recv(fd, &message, sizeof(struct message_s), 0)) < 0) {
            printf("Receive Error: %s (Errno:%d)\n", strerror(errno), errno);
            exit(1);
        }
        message.length = ntohl(message.length);

        int length = message.length - sizeof(struct message_s);
        createFile(filepath, length, fd);
        pthread_mutex_unlock(&lock);
    }
    //receive GET_REQUEST
    else if (message.type == 0xB1) {
        //lock this thread before getting filedata
        pthread_mutex_lock(&lock);

        //receive filename
        char filename[message.length - sizeof(struct message_s)];
        if ((count = recv(fd, filename, sizeof(filename), 0)) < 0) {
            printf("filename receive error: %s (Errno:%d)\n", strerror(errno), errno);
            exit(0);
        }
        char *filepath = getFilepath(filename);

        FILE *fp = fopen(filepath, "rb");
        if(fp) {
            //Send GET_REPLY 0xB2
            message = createMessage(0xB2, sizeof(struct message_s));
            if ((count = send(fd, &message, sizeof(struct message_s), 0)) < 0) {
                printf("GET_REPLY 0xB2 Error: %s (Errno:%d)\n", strerror(errno), errno);
                exit(1);
            }

            fseek(fp, 0, SEEK_END);    // seek to end of file
            int filesize = ftell(fp);  // get current file pointer
            fseek(fp, 0, SEEK_SET);

            //Send FILE_DATA
            message = createMessage(0xFF, sizeof(struct message_s) + filesize);
            if ((count = send(fd, &message, sizeof(struct message_s), 0)) < 0) {
                printf("FILE_DATA Error: %s (Errno:%d)\n", strerror(errno), errno);
                exit(1);
            }
            sendFile(fp, fd);
            fclose(fp);
        }
        else {
            //Send GET_REPLY 0xB3
            message = createMessage(0xB3, sizeof(struct message_s));
            if ((count = send(fd, &message, sizeof(struct message_s), 0)) < 0) {
                printf("GET_REPLY 0xB3 Error: %s (Errno:%d)\n", strerror(errno), errno);
                exit(1);
            }
        }
        pthread_mutex_unlock(&lock);
    }
    close(fd);
}


void main_loop(unsigned short port) {
    int fd, rc;
    struct sockaddr_in addr, tmp_addr;
    unsigned int addrlen = sizeof(struct sockaddr_in);
    pthread_t thread;

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
        perror("socket()");
        exit(1);
    }

    long val = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(long)) == -1) {
        perror("setsockopt()");
        exit(1);
    }

    memset(&addr, 0, sizeof(struct sockaddr_in));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    if (bind(fd, (struct sockaddr *) &addr, sizeof(addr)) == -1) {
        perror("bind()");
        exit(1);
    }
    if (listen(fd, 10) == -1) {
        perror("listen()");
        exit(1);
    }

    printf("[To stop the server: press Ctrl + C]\n");

    while (1) {
        int *accept_fd = (int *)malloc(sizeof(int));
        if ((*accept_fd = accept(fd, (struct sockaddr *) &tmp_addr, &addrlen)) == -1) {
            perror("accept()");
            exit(1);
        }

        if (rc = pthread_create(&thread, NULL, thr_func, accept_fd)) {
            fprintf(stderr, "error: pthread_create, rc: %d\n", rc);
            exit(1);
        }
    }
}

int main(int argc, char **argv) {
    unsigned short port;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s [port]\n", argv[0]);
        exit(1);
    }
    port = atoi(argv[1]);
    main_loop(port);

    return 0;
}
