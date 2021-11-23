// Non-canonical input processing

//TO-DO
//testar na lab e usar sleep
//funçao para stuffing/de e checksum
//https://www.youtube.com/watch?v=toS0RXNaTaE bitstuffing

//trama -> checksum -> bit stuffing
//bit destuffing -> checksum -> trama

#include <fcntl.h>
#include <stdio.h>
#include <signal.h> //alarm
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>
#include <stdbool.h>

#include "defines.h" //ficheiro com os defines

// Baudrate settings are defined in <asm/termbits.h>, which is
// included by <termios.h>
#define BAUDRATE B38400
#define _POSIX_SOURCE 1 // POSIX compliant source
#define FALSE 0
#define TRUE 1

#define BUF_SIZE 256

volatile int STOP = FALSE;

int atemptStart = FALSE;
int atemptCount = 0;
int state = 0; //estado
int checksum = 0;
bool stuffed;
int count=0;

char stuffing (char* buf) //obj: se a FLAG aparecer em A OU C fazer stuffing e retornar true caso ocorra stuffing e false caso contrario
{ // se aparecer 0x7e/FLAG/01111110 é modificado pela sequencia 0x7d0x5e(0x7d/1111101-0x5e/1011110) ou escape octate + resultado do ou exclusivo de 0x7e com 0x20
  char tmp_buf[BUF_SIZE];

  for(int i=0;i<BUF_SIZE;i++)
  {
    if(buf[i]==FLAG && count != 0)
    {
      count++;
      if(!checksum)
      {
        tmp_buf[count]=0x7d;
        count++;
        tmp_buf[count]=0x5e;
      }
      else
      {
        tmp_buf[count]=FLAG;
        break;
      }
    }
    else
    {
      tmp_buf[count]=buf[i];
      count++;
      if(buf[i] == BCC1)
      {
        checksum=1;
      }
    }
  }
  return *tmp_buf;
}

// Alarm function handler
void atemptHandler(int signal)
{
    atemptCount++;
    atemptStart = FALSE; //se não ele não entra no ciclo while de novo
    STOP = FALSE; // se não ele não entra no ciclo while de novo
    checksum = 0;
    count=0;
    if (atemptCount > 3)
    {
        printf("Dropping Connection\n");
    }
    else
    {
        printf("Atempt #%d\n", atemptCount);
    }
}

void errorcheck(int s)
{
  printf("An error ocurred in byte nº %d\n", state);
  checksum = -1; // error
}

void statemachine(char buf)// pode ser expandida para tratar mais tramas (I, DISC, ...)
{
  switch (state)
  {
    case 0: //start
    if (buf == FLAG)
    {
      state = 1;
      checksum = 0;
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
      checksum = 0;
      printf("A received\n");
    }
    else if (buf == FLAG)
    {
      state = 1;
      printf("FLAG received\n");
      //efetuar aqui o bit stuffing?
      errorcheck(state);
    }
    else
    {
      errorcheck(state);
    }
    break;
    case 2: //A
    if (buf == C2)
    {
      state = 3;
      checksum = 0;
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
    if (buf == (BCC2)) //BCC2
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

    // Set alarm function handler
    (void) signal(SIGALRM, atemptHandler); //atemptHandler -> funçao que vai ser envocada

    do
    {
      if (atemptStart == FALSE)
      {
        // Create frame to send
        char buf[BUF_SIZE] = {FLAG, A, C1, BCC1, FLAG};
        buf[count]=stuffing(buf);

        for (int i = 0; i < count; i++)
        {
            printf("%02X ", buf[i]);
        }

        int bytes = write(fd, buf, count);

        alarm(3);  // Set alarm to be triggered in 3s
        atemptStart = TRUE;

        printf("\nSET Frame Sent\n");
        printf("%d bytes written\n", bytes);
        printf("Waiting for UA\n");

        while (STOP == FALSE)
        {
            // VTIME = 30 para 3s e VMIN = 0 para incluir a possibilidade de nao receber nada
            // Alarm vai ter prevalencia q VTIME????
            int bytes = read(fd, buf, count);

            for (int i = 0; i < count; i++)
            {
              if(checksum != -1)
              {
                printf("%02X ", buf[i]);
                statemachine(buf[i]);
              }
            }
          	if (checksum == 2)
          	{
               printf("%d bytes received\n", bytes);
      		     printf("UA Frame Received Sucessfully\n");
               alarm(0); //desativa o alarme
      		     printf("End reception\n");
          	}
            STOP = TRUE;
        }
      }
      // codigo fica aqui preso a espera do 3s do alarme
    } while((state != 5) && (atemptCount < 4));

    // Restore the old port settings
    if (tcsetattr(fd, TCSANOW, &oldtio) == -1) {
        perror("tcsetattr");
        exit(-1);
    }

    close(fd);

    return 0;
}
