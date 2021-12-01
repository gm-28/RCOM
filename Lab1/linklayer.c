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
protocol_data pd;

volatile int STOP = FALSE;
int return_check = -1;           //-1 se falha 1 se não
int atemptStart = FALSE;
int atemptCount = 0;           //tentativas
int state = 0;                 //estado
int type = 0;                  //tipo de trama
int checksum = 0;
int fd;
int N=0;

//Error recovery for example, close the connection (DISC) re establish it (SET) and restart the process;
//-Event log (errors);
void protocol_stats(int r)
{
  printf("Printing File Statistics\n");
  if(r == TRANSMITTER)
  {
    printf("TRANSMITTER\n");
    printf("Transmissions:\n");
    printf("SET frames -> %d| I frames -> %d | DISC frames -> %d | UA frames -> %d\n", pd.num_set_a, pd.num_i_a, pd.num_disc_T, pd.num_ua_b);
    printf("Receptions:\n");
    printf("UA frames -> %d| RR frames -> %d | REJ frames -> %d |DISC frames -> %d\n", pd.num_ua_a, pd.num_rr, pd.num_rej, pd.num_disc_R);
    printf("Retransmissions:\n");
    printf("SET frames -> %d| I frames -> %d | DISC frames -> %d\n", pd.num_set_b, pd.num_i_b , pd.num_disc_Tb);
    printf("Timeouts -> %d\n", pd.tries);
  }
  else if(r == RECEIVER)
  {
    printf("RECEIVER\n");
    printf("Receptions:\n");
    printf("SET frames -> %d| I frames -> %d | DISC frames -> %d | UA frames -> %d\n", pd.num_set_a, pd.num_i_a , pd.num_disc_R, pd.num_ua_b);
    printf("Transmissions:\n");
    printf("UA frames -> %d| RR frames -> %d | REJ frames -> %d | DISC frames -> %d\n", pd.num_ua_a, pd.num_rr, pd.num_rej, pd.num_disc_T);
  }
  // printf("Error Recovery\n");
  // printf("%d\n", 1);
  // printf("Event Log\n");
  // printf("%d\n", 1);
}

char check_bcc2(char buf[],int bufsize)
{
    char bcc2 = buf[0];
    for(int i = 1; i < bufsize; i++)
    {
      bcc2 ^= buf[i];
      //printf("buf[%d]=%02X ",i,buf[i]);
      //printf(" bcc2 calculado: %02X \n",bcc2);
    }
    return bcc2;
}

//se aparecer 0x7e/FLAG/01111110 é modificado pela sequencia 0x7d0x5e(0x7d/1111101-0x5e/1011110)
//ou escape octate + resultado do ou exclusivo de 0x7e com 0x20
bool destuffing(char buf1,char buf2)
{
  return ((buf1 == 0x7d) && (buf2 == 0x5e));
}

//obj: se a FLAG aparecer em A OU C fazer stuffing e retornar true caso ocorra stuffing e false caso contrario
//se aparecer 0x7e/FLAG/01111110 é modificado pela sequencia 0x7d0x5e(0x7d/1111101-0x5e/1011110)
//ou escape octate + resultado do ou exclusivo de 0x7e com 0x20
//unsigned char stuffing (unsigned char* buf,int buf_size)
bool stuffing (char buf)
{
  return buf == FLAG;
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
          errorcheck(state);
        }
        else
        {
          errorcheck(state);
        }
        break;
      case 2: //A
        if ((buf == C2)) //&& (ll->role == TRANSMITTER))
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
        else if ((buf == (C_I0))||(buf == (C_RR0)))
        {
          N=1;
          state = 3;
          checksum++;//VERFICAR CHECKSUM
        }
        else if ((buf == (C_I1))||(buf == (C_RR1)))
        {
          N=0;
          state = 3;
          //printf("CRR received\n");
          checksum++;//VERFICAR CHECKSUM
        }
        else if(buf == C_DISC)
        {
          state = 3;
          //printf("C_DISC received\n");
          checksum++;//VERFICAR CHECKSUM
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
        else if ((buf == (BCC_I0))||(buf == (BCC_RR0)))
        {
          N=1;
          state = 4;
          //printf("BCC_I received\n");
        }
        else if ((buf == (BCC_I1))||(buf == (BCC_RR1)))
        {
          N=0;
          state = 4;
          //printf("BCC_RR received\n");
        }
        else if ((buf == (BCC_DISC)))
        {
          state = 4;
          //printf("BCC_DISC received\n");
        }
        else if (buf == FLAG)
        {
          state = 1;
          //printf("FLAG received\n");
          errorcheck(state);
        }
        else
        {
          printf("ERROOO?");
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
          if(buf == FLAG)
            {
              state = 5;
              checksum++;
            }
        }
        break;
      case 5: //End
        break;
    }
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
    newtio.c_cflag = BAUDRATE_DEFAULT | CS8 | CLOCAL | CREAD;
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

    //necessario e so funciona se pd e nao *pd pq?
    //sendo assim pq é q n podemos usar ll em vez de *ll
    pd.num_set_a = 0;
    pd.num_i_a = 0;
    pd.num_disc_T = 0;
    pd.num_ua_b = 0;
    pd.num_ua_a = 0;
    pd.num_rr = 0;
    pd.num_rej = 0;
    pd.num_disc_R = 0;
    pd.num_set_b = 0;
    pd.num_i_b = 0;
    pd.num_disc_Tb = 0;
    pd.tries = 0;

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
                pd.num_set_a++;
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
                        pd.num_ua_a++;
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
                pd.num_set_a++;
                unsigned char buf[TYPE1_SIZE] = {FLAG, A, C2, BCC2, FLAG};

                for (int i = 0; i < TYPE1_SIZE; i++)
                {
                  printf("%02X ", buf[i]);
                }

                int bytes = write(fd, buf, TYPE1_SIZE);
                printf("\nUA Frame Sent\n");
                pd.num_ua_a++;
                printf("%d bytes written\n", bytes);
                printf("Ending rx setup\n");
                STOP = TRUE;
                return_check = 1;
            }
        }
    }
    state = 0;          //reset da maquina de estados
    STOP = FALSE;       //reset
    atemptStart = FALSE;
    return return_check;
}

// Send data in buf with size bufSize.
// Return number of chars written, or "-1" on error.
int llwrite(char *buf, int bufSize)
{
    state=0;
    return_check = -1;
    type = 1;
    int data_s=0;

    // Create frame to send
    char frame[MAX_PAYLOAD_SIZE + 8];
    int framesize = 0;
    frame[0] = FLAG;
    frame[1] = A;
    if (!N)
    {
      frame[2] = C_I0;
      frame[3] = BCC_I0;
    }
    else
    {
      frame[2] = C_I1;
      frame[3] = BCC_I1;
    }
    framesize=4;
    for (int i = 0; i < bufSize; i++)
    {
      if(!stuffing(buf[i]))
      {
        frame[framesize] = buf[i];
        //printf("%02X ", frame[i]);
      }
      else
      {
        frame[framesize] = 0x7d;
        framesize++;
        frame[framesize]=0x5e;
      }
      framesize++;
      data_s++;
    }
    return_check=data_s;

    //notsure
    frame[framesize] = check_bcc2(buf,bufSize);
    if(stuffing(frame[framesize]))
    {
      frame[framesize] = 0x7d;
      framesize++;
      frame[framesize]=0x5e;
    }
    framesize++;
    frame[framesize] = FLAG;
    framesize++;

    do
    {
      if (atemptStart == FALSE)
      {
        write(fd, frame, framesize);

        //alarm(10);  // Set alarm to be triggered in 3s
        atemptStart = TRUE;

        printf("\nFrame Sent\n");
        pd.num_i_a++;
        // printf("%d bytes written\n",return_check);
        printf("Waiting for UA\n");

        while (STOP == FALSE)
        {
          int bytes = read(fd, buf, TYPE1_SIZE);
          for (int i = 0; i < TYPE1_SIZE; i++)
          {
            if(checksum != -1)
            {
              statemachine(buf[i],type);
            }
          }

          if (checksum == 2)
          {
            printf("%d bytes received\n", bytes);
            printf("UA Frame Received Sucessfully\n");
            pd.num_rr++;
            //alarm(0); //desativa o alarme
            printf("Ending tx setup\n");
          }
          STOP = TRUE;
        }
      }
    // codigo fica aqui preso a espera do 3s do alarme
  }while((state != 5) && (atemptCount < (ll->numTries+1)));

  atemptStart=FALSE;
  STOP=FALSE;
  printf("Return check last: %d \n",return_check);
  return return_check;
}

// Receive data in packet.
// Return number of chars read, or "-1" on error.
int llread(char *packet)
{
    char frame_data[MAX_PAYLOAD_SIZE + 8];
    state=0;
    return_check = -1;
    type = 2;
    int bytes_read = 0;
    int check=0;
    char tmp_buf[1];
    int bcc2_pos=0;
    int destf_count=0;
    int datasize=0;

    while (STOP == FALSE)
    {
      while (check !=2)
      {
        read(fd,tmp_buf,1);
        if(tmp_buf[0]==FLAG)
        {
          frame_data[bytes_read]=tmp_buf[0];
          check++;
          if(check ==2){
            break;
          }
        }
        else
        {
          frame_data[bytes_read]=tmp_buf[0];
        }
        bytes_read +=1;
      }

      //stuffing do bcc2 e verficação da posição do bcc2 para colocar os dados só até ao bcc2(não inclusive)
      char bcc2;
      int bcc2p;
      bcc2_pos=bytes_read;
      if(destuffing(frame_data[bytes_read-2],frame_data[bytes_read-1]))
      {
        destf_count++;
        bcc2_pos-=2;
        bcc2 = FLAG;
      }
      else
      {
        bcc2_pos-=1;
        //printf("bcc2 not stuffed %02X\n",frame_data[bcc2_pos]);
        bcc2 = frame_data[bcc2_pos];
      }
      bcc2p=bcc2_pos;

      //destuffing e colocação dos dados no pacote
      for (int i = 0; i < bytes_read+1; i++)//ir até flag bytesread=pos de flag
      {
        statemachine(frame_data[i],type);
        if((state == 4)&&(i>3)&&(i<bcc2p))
        {
          if(destuffing(frame_data[i],frame_data[i+1]))
          {
            destf_count++;
            //printf("Does destuffing happen? | %02X |%02X  ",frame_data[i],frame_data[i+1]);
            packet[datasize] = FLAG;
            i++;
            datasize++;
          }
          else
          {
            packet[datasize]=frame_data[i];
            //printf("datasize:%d \n  |",datasize);
            datasize++;
          }
        }
      }
      return_check = datasize;

      //isto é codigo de debug
      // printf("\n");
      // printf("de_stfu_count %d\n",destf_count);
      // printf("datasize 1 %d\n",datasize);
      // printf("-----------------------PACKET-----------------------\n");
      // for(int i=0;i<datasize;i++){
      //   printf("%02X ", packet[i]);
      // }
      // printf("----------------------------------------------------\n");
      // printf("\n");
      // printf("ulitma pos do packet %02X \n", packet[datasize-1]);
      // printf("\n");
      // printf("datasize  2 %d\n",datasize);

      if(bcc2 != check_bcc2(packet,datasize))
      {
        return_check = -1;
        printf("bcc2 original: %02X \n",bcc2);
        printf("bcc2 calculado2: %02X \n",check_bcc2(packet,datasize));
        STOP=TRUE;
        checksum = -1;
      }
      // printf("Checksum: %d \n",checksum);
      if (checksum == 2)
      {
        printf("Data Frame Received Sucessfully\n");
        pd.num_rr++;
        unsigned char buf[TYPE1_SIZE];
        buf[0] = FLAG;
        buf[1] = A;
        if (!N)
        {
          buf[2] = C_RR0;
          buf[3] = BCC_RR0;
        }
        else
        {
          buf[2] = C_RR1;
          buf[3] = BCC_RR1;
        }
        buf[4] = FLAG;

        for (int i = 0; i < TYPE1_SIZE; i++)
        {
          printf("%02X ", buf[i]);
        }

        int bytes = write(fd, buf, TYPE1_SIZE);
        printf("\nACK Frame Sent\n");
        pd.num_i_a++;
        printf("%d bytes written\n", bytes);
        STOP = TRUE;
      }
    }

    STOP = FALSE;
    printf("Return check last: %d \n",return_check);
    datasize=0;
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
                unsigned char buf[TYPE1_SIZE] = {FLAG, A, C_DISC, BCC_DISC, FLAG};
                unsigned char bufR[TYPE1_SIZE];

                for (int i = 0; i < TYPE1_SIZE; i++)
                {
                    printf("%02X ", buf[i]);
                }

                int bytes = write(fd, buf, TYPE1_SIZE);

                alarm(ll->timeOut);  // Set alarm to be triggered in 3s
                atemptStart = TRUE;

                printf("\nDISC Frame Sent\n");
                pd.num_disc_T++;
                printf("%d bytes written\n", bytes);
                printf("Waiting for DISC\n");

                while (STOP == FALSE)
                {
                    int bytes = read(fd, bufR, TYPE1_SIZE);

                    for (int i = 0; i < TYPE1_SIZE; i++)
                    {
                        if(checksum != -1)
                        {
                          printf("%02X ", bufR[i]);
                          statemachine(bufR[i],type);
                        }
                    }

                    if (checksum == 2)
                    {
                        printf("%d bytes received\n", bytes);
                        printf("DISC Frame Received Sucessfully\n");
                        pd.num_disc_R++;
                        alarm(0); //desativa o alarme
                    }
                    STOP = TRUE;
                }
                atemptStart=FALSE;
            }
            if (atemptStart == FALSE)//teste
            {
              unsigned char buf[TYPE1_SIZE] = {FLAG, A, C2, BCC2, FLAG};
              for (int i = 0; i < TYPE1_SIZE; i++)
              {
                  printf("%02X ", buf[i]);
              }

              int bytes = write(fd, buf, TYPE1_SIZE);
              //alarm(ll->timeOut);  // Set alarm to be triggered in 3s
              atemptStart = TRUE;

              printf("\nUA Frame Sent\n");
              pd.num_ua_b++;
              printf("%d bytes written\n", bytes);
              return_check = 1;
            }
        // codigo fica aqui preso a espera do 3s do alarme
      }while((state != 5) && (atemptCount < (ll->numTries+1)));
    }
    else if(ll->role == RECEIVER)
    {
        unsigned char buf[TYPE1_SIZE];
        printf("AM I HERE?\n");
        while (STOP == FALSE)
        {
            // Returns after 5 chars have been input
            printf("AM I HERE?2\n");
            int bytes = read(fd, buf, TYPE1_SIZE);

            for (int i = 0; i < TYPE1_SIZE; i++)
            {
              printf("%02X ", buf[i]);
              statemachine(buf[i],type);
            }

            printf("%d bytes received\n", bytes);

            if (checksum == 2)
            {
                printf("DISC Frame Received Sucessfully\n");
                pd.num_disc_R++;
                unsigned char buf[TYPE1_SIZE] = {FLAG, A, C_DISC, BCC_DISC, FLAG};

                for (int i = 0; i < TYPE1_SIZE; i++)
                {
                  printf("%02X ", buf[i]);
                }

                int bytes = write(fd, buf, TYPE1_SIZE);
                printf("\nDISC Frame Sent\n");
                pd.num_disc_T++;
                printf("%d bytes written\n", bytes);
                STOP = TRUE;
            }
            bytes = read(fd, buf, TYPE1_SIZE);

            for (int i = 0; i < TYPE1_SIZE; i++)
            {
              printf("%02X ", buf[i]);
              statemachine(buf[i],type);
            }

            printf("%d bytes received\n", bytes);

            if (checksum == 2)
            {
                printf("DISC Frame Received Sucessfully\n");
                pd.num_ua_a++;
                return_check = 1;
            }
        }
    }
    //Restore the old port settings perguntar stor
    //if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
    // {
    //      perror("tcsetattr");
    //      exit(-1);
    // }
    close(fd);

    if (showStatistics)
    {
      protocol_stats(ll->role);
    }

    return return_check;
}
