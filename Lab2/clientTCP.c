// (C)2000 FEUP

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#define SERVER_ADDR "192.168.28.96"
#define SERVER_PORT 6000

int main(int argc, char** argv)
{
    // Server address handling
    struct sockaddr_in server_addr;

    bzero((char*)&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(SERVER_ADDR);  // 32 bit Internet address network byte ordered
    server_addr.sin_port = htons(SERVER_PORT);  // Server TCP port must be network byte ordered

    // Open a TCP socket
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd < 0) {
        perror("socket()");
        exit(-1);
    }

    // Connect to the server
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect()");
        exit(-1);
    }

    // Send a string to the server
    char buf[] = "Test message in the TCP/IP stack\n";
    int bytes = write(sockfd, buf, strlen(buf));

    if (bytes < 0) {
        perror("write()");
        exit(-1);
    }

    printf("Sent %d bytes: %s\n", bytes, buf);

    // Close socket
    if (close(sockfd) < 0) {
        perror("close()");
        exit(-1);
    }

    return 0;
}
