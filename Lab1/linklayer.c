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
#include <stdbool.h>
#include "linklayer.h"

linkLayer *ll;
volatile int STOP = FALSE;
int return_check=-1;           //-1 se falha 1 se não

int atemptStart = FALSE;
int atemptCount = 0;
int state = 0; //estado
int checksum = 0;
bool stuffed;
int count=0;

char destuffing(char* buf){
  char tmp_buf[MAX_PAYLOAD_SIZE];                            // se aparecer 0x7e/FLAG/01111110 é modificado pela sequencia 0x7d0x5e(0x7d/1111101-0x5e/1011110) ou escape octate + resultado do ou exclusivo de 0x7e com 0x20

  for(int i=0;i<MAX_PAYLOAD_SIZE;i++){
    if(buf[i]== 0x7d && buf[i++]==0x5e && count != 0){
      tmp_buf[count]=FLAG;
      count++;
    }else if(buf[i]==FLAG && count != 0){
      tmp_buf[count]=FLAG;
      count++;
      break;
    }else{
      tmp_buf[count]=buf[i];
      count++;
    }
  }

  return *tmp_buf;
}

char stuffing (char* buf){ //obj: se a FLAG aparecer em A OU C fazer stuffing e retornar true caso ocorra stuffing e false caso contrario
  char tmp_buf[MAX_PAYLOAD_SIZE];                            // se aparecer 0x7e/FLAG/01111110 é modificado pela sequencia 0x7d0x5e(0x7d/1111101-0x5e/1011110) ou escape octate + resultado do ou exclusivo de 0x7e com 0x20

  for(int i=0;i<MAX_PAYLOAD_SIZE;i++){
    if(buf[i]==FLAG && count != 0){
      count++;
      if(!checksum){
        tmp_buf[count]=0x7d;
        count++;
        tmp_buf[count]=0x5e;
      }else{
        tmp_buf[count]=FLAG;
        break;
      }
    }else{
      tmp_buf[count]=buf[i];
      count++;
      if(buf[i] == BCC1){
        checksum=1;
      }
    }
  }

  return *tmp_buf;
}

void atemptHandler(int signal){
    atemptCount++;
    atemptStart = FALSE; //se não ele não entra no ciclo while de novo
    STOP = FALSE; // se não ele não entra no ciclo while de novo
    checksum = 0;
    count=0;
    if (atemptCount > 3){
        printf("Dropping Connection\n");
    }else{
        printf("Atempt #%d\n", atemptCount);
    }
}

void errorcheck(int s){
  printf("An error ocurred in byte nº %d\n", state);
  checksum = -1; // error
}

void statemachine(char buf){// pode ser expandida para tratar mais tramas (I, DISC, ...)
  switch (state){
    case 0: //start
      if (buf == FLAG){
        state = 1;
        checksum = 0;
        printf("FLAG1 received\n");
      }
      else{
        errorcheck(state);
      }
      break;
    case 1: //flag
      if (buf == A){
        state = 2;
        checksum = 0;
        printf("A received\n");
      }else if (buf == FLAG){
        state = 1;
        printf("FLAG received\n");
        //efetuar aqui o bit stuffing?
        errorcheck(state);
      }else{
        errorcheck(state);
      }
      break;
    case 2: //A
      if ((buf == C2) && (ll->role == TRANSMITTER)) {
        state = 3;
        checksum = 0;
        printf("C2 received\n");
        checksum++;
      }
      else if ((buf == C1) && (ll->role == RECEIVER))
      {
        state = 3;
        printf("C1 received\n");
        checksum++;
      }
      else if (buf == FLAG){
        state = 1;
        printf("FLAG received\n");
        errorcheck(state);
      }else{
        errorcheck(state);
      }
      break;
    case 3: //C
      if ((buf == (BCC2)) && (ll->role == TRANSMITTER)){ //BCC2
        state = 4;
        printf("BCC2 received\n");
      }
      if ((buf == (BCC1)) && (ll->role == RECEIVER)) //BCC1
      {
        state = 4;
        printf("BCC1 received\n");
      }
      else if (buf == FLAG){
        state = 1;
        printf("FLAG received\n");
        errorcheck(state);
      }else{
        errorcheck(state);
      }
      break;
    case 4: //BCC
      if (buf == FLAG){
        state = 5;
        printf("FLAG2 received\n");
        checksum++;
      }
      else{
        errorcheck(state);
      }
      break;
    case 5: //End
      break;
  }
}

// Open a connection using the "port" parameters defined in struct linkLayer.
// Return "1" on success or "-1" on error.
int llopen(linkLayer connectionParameters){
    ll = (linkLayer *) malloc(sizeof(linkLayer));
    ll->baudRate = connectionParameters.baudRate;
    ll->role = connectionParameters.role;
    ll->numTries = connectionParameters.numTries;
    ll->timeOut = connectionParameters.timeOut;
    sprintf(ll->serialPort,"%s",connectionParameters.serialPort);

    int fd = open(ll->serialPort, O_RDWR | O_NOCTTY );
    if (fd < 0) {
        perror(ll->serialPort);
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
    newtio.c_cflag = ll->baudRate | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;
    newtio.c_lflag = 0;

    if(ll->role == TRANSMITTER)
    {
      newtio.c_cc[VTIME] = 30; // Inter-character timer unused
      newtio.c_cc[VMIN] = 0;  // Blocking read until 5 chars received
    }
    else
    {
      newtio.c_cc[VTIME] = 0;  // Inter-character timer unused
      newtio.c_cc[VMIN]  = 5;  // Blocking read until 5 chars received
    }

    tcflush(fd, TCIOFLUSH);

    // Set new port settings
    if (tcsetattr(fd, TCSANOW, &newtio) == -1) {
        perror("tcsetattr");
        exit(-1);
    }

    printf("New termios structure set\n");

    // Set alarm function handler
    (void) signal(SIGALRM, atemptHandler); //atemptHandler -> funçao que vai ser envocada

    if(ll->role == TRANSMITTER){                //writenoncanonical
        do{
            if (atemptStart == FALSE){
                // Create frame to send
                char buf[MAX_PAYLOAD_SIZE] = {FLAG, A, C1, BCC1, FLAG};
                buf[count]=stuffing(buf);

                for (int i = 0; i < count; i++){
                    printf("%02X ", buf[i]);
                }

                int bytes = write(fd, buf, count);

                alarm(3);  // Set alarm to be triggered in 3s
                atemptStart = TRUE;

                printf("\nSET Frame Sent\n");
                printf("%d bytes written\n", bytes);
                printf("Waiting for UA\n");

                while (STOP == FALSE){
                    // VTIME = 30 para 3s e VMIN = 0 para incluir a possibilidade de nao receber nada
                    // Alarm vai ter prevalencia q VTIME????

                    int bytes = read(fd, buf, count);

                    for (int i = 0; i < count; i++){
                        if(checksum != -1){
                            printf("%02X ", buf[i]);
                            statemachine(buf[i]);
                            printf("TESTE ");
                        }
                    }

                    if (checksum == 2){
                        printf("%d bytes received\n", bytes);
                        printf("UA Frame Received Sucessfully\n");
                        alarm(0); //desativa o alarme
                        printf("End reception\n");
                        return_check = 1;
                    }
                    STOP = TRUE;
                }
            }
        // codigo fica aqui preso a espera do 3s do alarme
        }while((state != 5) && (atemptCount < 4));
    }else{                                                      //noncanonical
        char buf[MAX_PAYLOAD_SIZE];
        while (STOP == FALSE){
            // Returns after 5 chars have been input
            int bytes = read(fd, buf, MAX_PAYLOAD_SIZE);

            buf[count] = destuffing(buf);
            printf("T %d T\n",count);

            for (int i = 0; i < count; i++){
                printf("%02X ", buf[i]);
                statemachine(buf[i]);
            }

            printf("%d bytes received\n", bytes);

            if (checksum == 2){
                printf("SET Frame Received Sucessfully\n");
                char buf[MAX_PAYLOAD_SIZE] = {FLAG, A, C2, BCC2, FLAG};

                for (int i = 0; i < count; i++){
                        printf("%02X ", buf[i]);
                }

                int bytes = write(fd, buf, count);
                printf("\nUA Frame Sent\n");
                printf("%d bytes written\n", bytes);
                printf("End reception\n");
                STOP = TRUE;
                return_check = 1;
            }
        }
    }
    return return_check;

    //llclose maybe
    // Restore the old port settings
    // if (tcsetattr(fd, TCSANOW, &oldtio) == -1){
    //     perror("tcsetattr");
    //     exit(-1);
    // }
}

// Send data in buf with size bufSize.
// Return number of chars written, or "-1" on error.
int llwrite(char *buf, int bufSize){}

// Receive data in packet.
// Return number of chars read, or "-1" on error.
int llread(char *packet){}

// Close previously opened connection.
// if showStatistics == TRUE, link layer should print statistics in the console on close.
// Return "1" on success or "-1" on error.
int llclose(int showStatistics){}
