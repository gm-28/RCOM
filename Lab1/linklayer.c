#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>
#include "defines.h"            //ficheiro com os defines
#include <stdbool.h>
#include "linklayer.h"

linkLayer *ll;

volatile int STOP = FALSE;
int return_check = -1;           //-1 se falha 1 se não
int atemptStart = FALSE;
int atemptCount = 0;           //tentativas
int state = 0;                 //estado
int type = 0;                  //tipo de trama
int checksum = 0;
bool stuffed;
int datasize = 0;
int fd;


char check_bcc2(char buf[],int bufsize)
{
    char bcc2 = buf[0];
    for(int i = 1; i < bufsize; i++)
    {
      bcc2 ^= buf[i];
    }
    return bcc2;
}

//se aparecer 0x7e/FLAG/01111110 é modificado pela sequencia 0x7d0x5e(0x7d/1111101-0x5e/1011110)
//ou escape octate + resultado do ou exclusivo de 0x7e com 0x20
unsigned char destuffing(unsigned char* buf)
{
    unsigned char tmp_buf[datasize];

    for(int i = 0; i < datasize; i++)
    {
      if((buf[i] == 0x7d) && (tmp_buf[i++] == 0x5e))
      {
        tmp_buf[i] = FLAG;
      }
      else
      {
        tmp_buf[i] = buf[i];
      }
    }
    return *tmp_buf;
}

//obj: se a FLAG aparecer em A OU C fazer stuffing e retornar true caso ocorra stuffing e false caso contrario
//se aparecer 0x7e/FLAG/01111110 é modificado pela sequencia 0x7d0x5e(0x7d/1111101-0x5e/1011110)
//ou escape octate + resultado do ou exclusivo de 0x7e com 0x20
//unsigned char stuffing (unsigned char* buf,int buf_size)
unsigned char stuffing (unsigned char* buf)
{
    unsigned char tmp_buf[MAX_PAYLOAD_SIZE];

    for(int i = 0; i < MAX_PAYLOAD_SIZE; i++)
    {
      if(buf[i] == FLAG)
      {
        tmp_buf[i] = 0x7d;
        i++;
        tmp_buf[i] = 0x5e;
      }
      else
      {
        tmp_buf[i] = buf[i];
      }
      datasize = i;
    }
    return *tmp_buf;
}

//mudar o timeout usando as flags no linklayer.h????????
void atemptHandler(int signal)
{
    atemptCount++;
    atemptStart = FALSE; //se não ele não entra no ciclo while de novo
    STOP = FALSE; // se não ele não entra no ciclo while de novo
    checksum = 0;
    if (atemptCount > ll->numTries)
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

// pode ser expandida para tratar mais tramas (I, DISC, ...)
void statemachine(unsigned char buf, int type)
{
    //SET & UA Frames
    switch (state)
    {
        case 0: //start
          if (buf == FLAG)
          {
            state = 1;
            checksum = 0;
            //printf("FLAG1 received\n");
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
            //printf("A received\n");
          }
          else if (buf == FLAG)
          {
            state = 1;
            //printf("FLAG received\n");
            //efetuar aqui o bit stuffing?
            errorcheck(state);
          }
          else
          {
            errorcheck(state);
          }
          break;
        case 2: //A
          if ((buf == C2) && (ll->role == TRANSMITTER))
          {
            state = 3;
            checksum = 0;
            //printf("C2 received\n");
            checksum++;
          }
          else if ((buf == C1) && (ll->role == RECEIVER))
          {
            state = 3;
            //printf("C1 received\n");
            checksum++;
          }
          else if (buf == FLAG)
          {
            state = 1;
            //printf("FLAG received\n");
            errorcheck(state);
          }
          else
          {
            errorcheck(state);
          }
          break;
        case 3: //C
          if ((buf == (BCC2)))
          {
            state = 4;
            //printf("BCC2 received\n");
          }
          else if ((buf == (BCC1)))
          {
            state = 4;
            //printf("BCC1 received\n");
          }
          else if (buf == FLAG)
          {
            state = 1;
            //printf("FLAG received\n");
            errorcheck(state);
          }
          else
          {
            errorcheck(state);
          }
          break;
        case 4: //BCC
          if (type == 1)
          {
            if (buf == FLAG)
            {
              state = 5;
              //printf("FLAG2 received\n");
              checksum++;
            }
            else
            {
              errorcheck(state);
            }
          }
          else if (type == 2)
          {
            if (buf == FLAG)
              {
                state = 5;
                printf("FLAG2 received\n");
                checksum++;
              }
          }
          break;
        case 5: //End
          break;
      }
    // //Data Frames stor tinha falado numa funçao para o calculo do BCC??????
    // else if (type == 2)
    // {
    //   switch (state)
    //   {
    //       case 0: //start
    //         if (buf == FLAG)
    //         {
    //           state = 1;
    //           checksum = 0;
    //           printf("FLAG1 received\n");
    //         }
    //         else
    //         {
    //           errorcheck(state);
    //         }
    //         break;
    //       case 1: //flag
    //         if (buf == A)
    //         {
    //           state = 2;
    //           checksum = 0;
    //           printf("A received\n");
    //         }
    //         else if (buf == FLAG)
    //         {
    //           state = 1;
    //           printf("FLAG received\n");
    //           //efetuar aqui o bit stuffing?
    //           errorcheck(state);
    //         }
    //         else
    //         {
    //           errorcheck(state);
    //         }
    //         break;
    //       case 2: //A
    //         if ((buf == C2) && (ll->role == TRANSMITTER))
    //         {
    //           state = 3;
    //           checksum = 0;
    //           printf("C2 received\n");
    //           checksum++;
    //         }
    //         else if ((buf == C1) && (ll->role == RECEIVER))
    //         {
    //           state = 3;
    //           printf("C1 received\n");
    //           checksum++;
    //         }
    //         else if (buf == FLAG)
    //         {
    //           state = 1;
    //           printf("FLAG received\n");
    //           errorcheck(state);
    //         }
    //         else
    //         {
    //           errorcheck(state);
    //         }
    //         break;
    //       case 3: //C
    //         if ((buf == (BCC2)))
    //         {
    //           state = 4;
    //           printf("BCC2 received\n");
    //         }
    //         else if ((buf == (BCC1)))
    //         {
    //           state = 4;
    //           printf("BCC1 received\n");
    //         }
    //         else if (buf == FLAG)
    //         {
    //           state = 1;
    //           printf("FLAG received\n");
    //           errorcheck(state);
    //         }
    //         else
    //         {
    //           errorcheck(state);
    //         }
    //         break;
    //       case 4://data
    //         if (buf == FLAG)
    //         {
    //           state = 5;
    //           printf("FLAG2 received\n");
    //           checksum++;
    //         }
    //         break;
    //       case 5: //End
    //         break;
    //     }
    // }
    //Disc Frames etc....
    //}
}

// Open a connection using the "port" parameters defined in struct linkLayer.
// Return "1" on success or "-1" on error.
int llopen(linkLayer connectionParameters)
{
    ll = (linkLayer *) malloc(sizeof(linkLayer));
    ll->baudRate = connectionParameters.baudRate;
    ll->role = connectionParameters.role;
    ll->numTries = connectionParameters.numTries;
    ll->timeOut = connectionParameters.timeOut;
    sprintf(ll->serialPort,"%s",connectionParameters.serialPort);

    fd = open(ll->serialPort, O_RDWR | O_NOCTTY );
    if (fd < 0)
    {
        perror(ll->serialPort);
        exit(-1);
    }
    struct termios oldtio;
    struct termios newtio;
    // Save current port settings
    if (tcgetattr(fd, &oldtio) == -1)
    {
        perror("tcgetattr");
        exit(-1);
    }

    // Clear struct for new port settings
    bzero(&newtio, sizeof(newtio));
    newtio.c_cflag = ll->baudRate | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;
    newtio.c_lflag = 0;

    if(ll->role == TRANSMITTER)
    {
      newtio.c_cc[VTIME] = 40; // Inter-character timer unused
      newtio.c_cc[VMIN] = 0;  // Blocking read until 5 chars received (incluir a possibilidade de nao receber nada)
    }
    else if(ll->role == RECEIVER)
    {
      newtio.c_cc[VTIME] = 0;  // Inter-character timer unused
      newtio.c_cc[VMIN]  = 5;  // Blocking read until 5 chars received
    }

    tcflush(fd, TCIOFLUSH);

    // Set new port settings
    if (tcsetattr(fd, TCSANOW, &newtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    printf("New termios structure set\n");

    // Set alarm function handler -> atemptHandler -> funçao que vai ser envocada
    (void) signal(SIGALRM, atemptHandler);

    type = 1;

    if(ll->role == TRANSMITTER)
    {
        do
        {
            if (atemptStart == FALSE)
            {
                // Create frame to send
                unsigned char buf[TYPE1_SIZE] = {FLAG, A, C1, BCC1, FLAG};

                for (int i = 0; i < TYPE1_SIZE; i++)
                {
                    printf("%02X ", buf[i]);
                }

                int bytes = write(fd, buf, TYPE1_SIZE);

                alarm(ll->timeOut);  // Set alarm to be triggered in 3s
                atemptStart = TRUE;

                printf("\nSET Frame Sent\n");
                printf("%d bytes written\n", bytes);
                printf("Waiting for UA\n");

                while (STOP == FALSE)
                {
                    int bytes = read(fd, buf, TYPE1_SIZE);

                    for (int i = 0; i < TYPE1_SIZE; i++)
                    {
                        if(checksum != -1)
                        {
                          printf("%02X ", buf[i]);
                          statemachine(buf[i],type);
                        }
                    }

                    if (checksum == 2)
                    {
                        printf("%d bytes received\n", bytes);
                        printf("UA Frame Received Sucessfully\n");
                        alarm(0); //desativa o alarme
                        printf("Ending tx setup\n");
                        return_check = 1;
                    }
                    STOP = TRUE;
                }
            }
        // codigo fica aqui preso a espera do 3s do alarme
      }while((state != 5) && (atemptCount < (ll->numTries+1)));
    }
    else if(ll->role == RECEIVER)
    {
        unsigned char buf[TYPE1_SIZE];
        while (STOP == FALSE)
        {
            // Returns after 5 chars have been input
            int bytes = read(fd, buf, TYPE1_SIZE);

            for (int i = 0; i < TYPE1_SIZE; i++)
            {
              printf("%02X ", buf[i]);
              statemachine(buf[i],type);
            }

            printf("%d bytes received\n", bytes);

            if (checksum == 2)
            {
                printf("SET Frame Received Sucessfully\n");
                unsigned char buf[TYPE1_SIZE] = {FLAG, A, C2, BCC2, FLAG};

                for (int i = 0; i < TYPE1_SIZE; i++)
                {
                  printf("%02X ", buf[i]);
                }

                int bytes = write(fd, buf, TYPE1_SIZE);
                printf("\nUA Frame Sent\n");
                printf("%d bytes written\n", bytes);
                printf("Ending rx setup\n");
                STOP = TRUE;
                return_check = 1;
            }
        }
    }
    state = 0;          //reset da maquina de estados
    STOP = FALSE;       //reset
    return return_check;
}

// Send data in buf with size bufSize.
// Return number of chars written, or "-1" on error.
int llwrite(char *buf, int bufSize)
{
    return_check = -1;
    type = 1;
    char *data = (char *)malloc(bufSize);
    data = stuffing(buf);

    // Create frame to send
    char frame[MAX_PAYLOAD_SIZE + 6];
    int framesize = 0;
    frame[0] = FLAG;
    frame[1] = A;
    frame[2] = C_I;
    frame[3] = BCC_I;
    for (int i = 0; i < datasize; i++)
    {
      frame[i+4] = data[i];
      framesize = i+4;
      printf("%02X ", frame[i+4]);
    }
    framesize++;
    frame[framesize] = check_bcc2(data,datasize);
    framesize++;
    frame[framesize] = FLAG;

    do
    {
      if (atemptStart == FALSE)
      {
        return_check = write(fd, frame, framesize);
        //return check caso read falhe??

        alarm(ll->timeOut);  // Set alarm to be triggered in 3s
        atemptStart = TRUE;

        printf("\nSET Frame Sent\n");
        printf("%d bytes written\n",return_check);
        printf("Waiting for UA\n");

        while (STOP == FALSE)
        {
          int bytes = read(fd, buf, TYPE1_SIZE);
          for (int i = 0; i < TYPE1_SIZE; i++)
          {
            if(checksum != -1)
            {
              printf("%02X ", buf[i]);
              statemachine(buf[i],type);
            }
          }

          if (checksum == 2)
          {
            printf("%d bytes received\n", bytes);
            printf("UA Frame Received Sucessfully\n");
            alarm(0); //desativa o alarme
            printf("Ending tx setup\n");
          }
          STOP = TRUE;
        }
      }
    // codigo fica aqui preso a espera do 3s do alarme
  }while((state != 5) && (atemptCount < (ll->numTries+1)));

    return return_check;
}

// Receive data in packet.
// Return number of chars read, or "-1" on error.
int llread(char *packet)
{
    char frame_data[MAX_PAYLOAD_SIZE + 6];
    return_check = -1;
    type = 2;
    int bytes_read = 0;
    int check;
    while (STOP == FALSE)
    {
      while (bytes_read > 0)
      {
        bytes_read++;
        check = read(fd,frame_data[bytes_read],1);
        //check n é usado em nada??????????
      }
      return_check = bytes_read;
      //return check em erro de write????

      for (int i = 0; i < bytes_read; i++)
      {
        printf("%02X ", frame_data[i]);
        statemachine(frame_data[i],type);
        if(state == 4)
        {
          packet[datasize] = frame_data[i];
          datasize++;
        }
      }
      char bcc2 = packet[datasize];
      datasize--;
      packet = destuffing(packet);
      if(bcc2 == check_bcc2(packet,datasize))
      {
        return_check = -1;
        checksum = -1;
      }

      printf("%d bytes received\n", bytes_read);

      if (checksum == 2)
      {
        printf("Data Frame Received Sucessfully\n");
        unsigned char buf[TYPE1_SIZE] = {FLAG, A, C_RR, BCC_RR, FLAG};

        for (int i = 0; i < TYPE1_SIZE; i++)
        {
          printf("%02X ", buf[i]);
        }

        int bytes = write(fd, buf, TYPE1_SIZE);
        printf("\nACK Frame Sent\n");
        printf("%d bytes written\n", bytes);
        STOP = TRUE;
      }
    }
    return return_check;
}

// Close previously opened connection.
// if showStatistics == TRUE, link layer should print statistics in the console on close.
// Return "1" on success or "-1" on error.
int llclose(int showStatistics)
{
    return_check = -1;
    type = 1;

    if(ll->role == TRANSMITTER)
    {
        do
        {
            if (atemptStart == FALSE)
            {
                // Create frame to send
                unsigned char buf[TYPE1_SIZE] = {FLAG, A, C1, BCC1, FLAG};

                for (int i = 0; i < TYPE1_SIZE; i++)
                {
                    printf("%02X ", buf[i]);
                }

                int bytes = write(fd, buf, TYPE1_SIZE);

                alarm(ll->timeOut);  // Set alarm to be triggered in 3s
                atemptStart = TRUE;

                printf("\nSET Frame Sent\n");
                printf("%d bytes written\n", bytes);
                printf("Waiting for UA\n");

                while (STOP == FALSE)
                {
                    int bytes = read(fd, buf, TYPE1_SIZE);

                    for (int i = 0; i < TYPE1_SIZE; i++)
                    {
                        if(checksum != -1)
                        {
                          printf("%02X ", buf[i]);
                          statemachine(buf[i],type);
                        }
                    }

                    if (checksum == 2)
                    {
                        printf("%d bytes received\n", bytes);
                        printf("UA Frame Received Sucessfully\n");
                        alarm(0); //desativa o alarme
                        printf("Ending tx setup\n");
                        return_check = 1;
                    }
                    STOP = TRUE;
                }
            }
        // codigo fica aqui preso a espera do 3s do alarme
      }while((state != 5) && (atemptCount < (ll->numTries+1)));
    }
    else if(ll->role == RECEIVER)
    {
        unsigned char buf[TYPE1_SIZE];
        while (STOP == FALSE)
        {
            // Returns after 5 chars have been input
            int bytes = read(fd, buf, TYPE1_SIZE);

            for (int i = 0; i < TYPE1_SIZE; i++)
            {
              printf("%02X ", buf[i]);
              statemachine(buf[i],type);
            }

            printf("%d bytes received\n", bytes);

            if (checksum == 2)
            {
                printf("SET Frame Received Sucessfully\n");
                unsigned char buf[TYPE1_SIZE] = {FLAG, A, C2, BCC2, FLAG};

                for (int i = 0; i < TYPE1_SIZE; i++)
                {
                  printf("%02X ", buf[i]);
                }

                int bytes = write(fd, buf, TYPE1_SIZE);
                printf("\nUA Frame Sent\n");
                printf("%d bytes written\n", bytes);
                printf("Ending rx setup\n");
                STOP = TRUE;
                return_check = 1;
            }
        }
    }
    //llclose maybe
    // Restore the old port settings
    // if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
    //{
    //     perror("tcsetattr");
    //     exit(-1);
    // }
    //close(fd);
    return return_check;
}
