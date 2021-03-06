// Non-canonical input processing

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>

// Baudrate settings are defined in <asm/termbits.h>, which is
// included by <termios.h>
#define BAUDRATE B38400
#define _POSIX_SOURCE 1 // POSIX compliant source
#define FALSE 0
#define TRUE 1

#define BUF_SIZE 256

#define FLAG 0X7E	//01111110
#define A 0X02		//00000010 Transmitter
#define C 0X07		//00000111 UA
#define BCC (A^C)	//XOR(A,C)

volatile int STOP = FALSE;

void writebuf(int fd, char* buf)
{   
    for (int i = 0; i < strlen(buf); i++)
    {
	printf("%02X ", buf[i]);
    }
    int bytes = write(fd, buf, strlen(buf)); 
    printf("\nUA Frame Sent\n");
    printf("%d bytes written\n", bytes);
}

void readbuf(int fd, char* buf)
{
    while (STOP == FALSE) 
    {
        // Returns after 5 chars have been input
        int bytes = read(fd, buf, BUF_SIZE);
        buf[bytes] = '\0';  // Set end of string to '\0', so we can printf

	for (int i = 0; i < strlen(buf); i++)
    	{
        	printf("%02X ", buf[i]);
    	}
        printf("\n%d bytes received\n", bytes);
    	if (buf[2] == 0X03) //mudar para outro define (noutro ficheiro.h?)
    	{
		printf("SET Frame Received\n");
		
		char buf[BUF_SIZE] = {FLAG, A, C, BCC, FLAG};
		
		writebuf(fd, buf);
		
		printf("End reception\n");	
    		STOP = TRUE;
    	}
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

    // VTIME e VMIN should be changed in order to protect with a
    // timeout the reception of the following character(s)

    // Now clean the line and activate the settings for the port
    // tcflush() discards data written to the object referred  to
    // by  fd but not transmitted, or data received but not read,
    // depending on the value of queue_selector:
    //   TCIFLUSH - flushes data received but not read.
    tcflush(fd, TCIOFLUSH);

    // Set new port settings
    if (tcsetattr(fd, TCSANOW, &newtio) == -1) {
        perror("tcsetattr");
        exit(-1);
    }

    printf("New termios structure set\n");
    
    // Loop for input
    char buf[BUF_SIZE + 1]; // +1: Save space for the final '\0' char

   readbuf(fd, buf);

   // falta ler/enviar um a um???????
   // usar um switch????
   // necessario criar 2 buffers?????

    // The while() cycle should be changed in order to respect the specifications
    // of the protocol indicated in the Lab guide

    // Restore the old port settings
    if (tcsetattr(fd, TCSANOW, &oldtio) == -1) {
        perror("tcsetattr");
        exit(-1);
    }

    close(fd);

    return 0;
}
