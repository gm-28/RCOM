
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>

#include "defines.h" //ficheiro com os defines

typedef struct linkLayer {
    char serialPort[50]; //Device /dev/ttySx, x = 0, 1 */
    int role; // defines the role of the program: 0 == Transmitter, 1 == Receiver
    int baudRate; //Transmission speed
    int numTries; //Number of retransmissions in case of failures
    int timeOut; //Timeout value: 1 s
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

int main(int argc, char** argv)
{
  ????
}

// Open a connection using the "port" parameters defined in struct linkLayer.
// Return "1" on success or "-1" on error.
int llopen(linkLayer connectionParameters);
{
  // Program usage: Uses either COM1 or COM2
  // ttyS10(1) para cable e ttyS0 para lab
  if ((argc < 2) ||
      ((strcmp("/dev/ttyS10", argv[1]) != 0) &&
       (strcmp("/dev/ttyS11", argv[1]) != 0)))
  {
      printf("Incorrect program usage\n"
             "Usage: %s <SerialPort>\n"
             "Example: %s /dev/ttyS1\n",
             argv[0], argv[0]);
      exit(1);
  }

  // Open serial port device for reading and writing and not as controlling tty
  // because we don't want to get killed if linenoise sends CTRL-C.
  int fd = open(argv[1], O_RDWR | O_NOCTTY );
  if (fd < 0) {
      perror(argv[1]);
      exit(-1);
  }

  struct termios oldtio;
  struct termios newtio;

  // Save current port settings
  if (tcgetattr(fd, &oldtio) == -1) {
      perror("tcgetattr");
      exit(-1);
  }

  // Clear struct for new port settings
  bzero(&newtio, sizeof(newtio));

  newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
  newtio.c_iflag = IGNPAR;
  newtio.c_oflag = 0;

  // Set input mode (non-canonical, no echo,...)
  newtio.c_lflag = 0;
  newtio.c_cc[VTIME] = 30; // Inter-character timer unused
  newtio.c_cc[VMIN] = 0;  // Blocking read until 5 chars received

  //   TCIFLUSH - flushes data received but not read.
  tcflush(fd, TCIOFLUSH);

  // Set new port settings
  if (tcsetattr(fd, TCSANOW, &newtio) == -1) {
      perror("tcsetattr");
      exit(-1);
  }

  printf("New termios structure set\n");
}
