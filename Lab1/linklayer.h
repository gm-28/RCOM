#ifndef _LINKLAYER_H_
#define _LINKLAYER_H_

#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>

typedef struct linkLayer {
    char serialPort[50];
    int role; // defines the role of the program: 0 == Transmitter, 1 == Receiver
    int baudRate;
    int numTries;
    int timeOut;
} linkLayer;

// ROLE
#define NOT_DEFINED -1
#define TRANSMITTER 0
#define RECEIVER 1

// SIZE of maximum acceptable payload; maximum number of bytes that application layer should send to link layer
#define MAX_PAYLOAD_SIZE 1000

// CONNECTION default values
#define BAUDRATE_DEFAULT B38400
#define MAX_RETRANSMISSIONS_DEFAULT 3
#define TIMEOUT_DEFAULT 4

// MISC
#define _POSIX_SOURCE 1  // POSIX compliant source

#define FALSE 0
#define TRUE 1

// Open a connection using the "port" parameters defined in struct linkLayer.
// Return "1" on success or "-1" on error.
int llopen(linkLayer connectionParameters);

// Send data in buf with size bufSize.
// Return number of chars written, or "-1" on error.
int llwrite(char *buf, int bufSize);

// Receive data in packet.
// Return number of chars read, or "-1" on error.
int llread(char *packet);

// Close previously opened connection.
// if showStatistics == TRUE, link layer should print statistics in the console on close.
// Return "1" on success or "-1" on error.
int llclose(int showStatistics);

#endif // _LINKLAYER_H_
