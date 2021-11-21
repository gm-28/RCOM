// Non-canonical input processing

//TO-DO
//testar na lab e usar sleep
//funçao para stuffing/de e checksum

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>

#include "defines.h" //ficheiro com os defines

// Baudrate settings are defined in <asm/termbits.h>, which is
// included by <termios.h>
#define BAUDRATE B38400
#define _POSIX_SOURCE 1 // POSIX compliant source
#define FALSE 0
#define TRUE 1

#define BUF_SIZE 256

volatile int STOP = FALSE;

int state = 0; //estado
int checksum = 0;

void errorcheck(int s)
{
  printf("An error ocurred at the %d byte\n", state);
  state = 0;
  exit(-1);
}

void statemachine(char buf)
{
  switch (state)
  {
    case 0: //start
    if (buf == FLAG)
    {
      state = 1;
      printf("FLAG1 received\n");
    }
    else
    {
      errorcheck(state);
    }
    break;
    case 1: //flag
    if (buf == A)
    {
      state = 2;
      printf("A received\n");
    }
    else if (buf == FLAG)
    {
      state = 1;
      printf("FLAG received\n");
      errorcheck(state);
    }
    else
    {
      errorcheck(state);
    }
    break;
    case 2: //A
    if (buf == C1)
    {
      state = 3;
      printf("C received\n");
      checksum++;
    }
    else if (buf == FLAG)
    {
      state = 1;
      printf("FLAG received\n");
      errorcheck(state);
    }
    else
    {
      errorcheck(state);
    }
    break;
    case 3: //C
    if (buf == (BCC1)) //BCC1
    {
      state = 4;
      printf("BCC received\n");
    }

    else if (buf == FLAG)
    {
      state = 1;
      printf("FLAG received\n");
      errorcheck(state);
    }
    else
    {
      errorcheck(state);
    }
    break;
    case 4: //BCC
    if (buf == FLAG)
    {
      state = 5;
      printf("FLAG2 received\n");
      checksum++;
    }
    else
    {
      errorcheck(state);
    }
    break;
    case 5: //End
    break;
  }
}

int main(int argc, char** argv)
{
    // Program usage: Uses either COM1 or COM2
    // ttyS10(1) para cable e ttyS0 para lab
    if ((argc < 2) ||
        ((strcmp("/dev/ttyS10", argv[1]) != 0) &&
         (strcmp("/dev/ttyS11", argv[1]) != 0) ))
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
    newtio.c_cc[VTIME] = 0;  // Inter-character timer unused
    newtio.c_cc[VMIN]  = 5;  // Blocking read until 5 chars received

    //   TCIFLUSH - flushes data received but not read.
    tcflush(fd, TCIOFLUSH);

    // Set new port settings
    if (tcsetattr(fd, TCSANOW, &newtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    printf("New termios structure set\n");

    // Loop for input
    char buf[BUF_SIZE + 1]; // +1: Save space for the final '\0' char

    while (STOP == FALSE)
    {
        // Returns after 5 chars have been input
        int bytes = read(fd, buf, BUF_SIZE);
        buf[bytes] = '\0';  // Set end of string to '\0', so we can printf

        for (int i = 0; i < strlen(buf); i++)
        {
           printf("%02X ", buf[i]);
           statemachine(buf[i]);
        }
        printf("%d bytes received\n", bytes);
        if (checksum == 2)
        {
            printf("SET Frame Received Sucessfully\n");

            char buf[BUF_SIZE] = {FLAG, A, C2, BCC2, FLAG};

            for (int i = 0; i < strlen(buf); i++)
            {
        	     printf("%02X ", buf[i]);
            }
            int bytes = write(fd, buf, strlen(buf));
            printf("\nUA Frame Sent\n");
            printf("%d bytes written\n", bytes);

            printf("End reception\n");
            STOP = TRUE;
        }
    }

    // Restore the old port settings
    if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    close(fd);

    return 0;
}
