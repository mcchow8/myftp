#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <dirent.h>
#include <errno.h>
#include <netinet/in.h>	// "struct sockaddr_in"
#include <arpa/inet.h>	// "in_addr_t"

#define SEND_BUFFER_SIZE 1048576    //use this when create a buffer for send()
#define RECV_BUFFER_SIZE 65536      //use this when create a buffer for recv()

struct message_s {
    unsigned char protocol[5];  /* protocol string (5 bytes) */
    unsigned char type;         /* type (1 byte) */
    unsigned int length;        /* length (header + payload) (4 bytes) */
} __attribute__ ((packed));

struct message_s createMessage(unsigned char type, unsigned int length);
void createFile(char *filepath, int length, int fd);
void sendFile(FILE *fp, int fd);