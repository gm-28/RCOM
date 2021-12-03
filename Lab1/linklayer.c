#include "linklayer.h"
#include "defines.h"            //ficheiro com os defines

#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>
#include <stdbool.h>            //biblioteca para bool
#include <time.h>               //biblioteca para clock

linkLayer *ll;
protocol_data pd;

volatile int STOP = FALSE;     //inicio de receção
int return_check = -1;         //-1 se falha 1 se não
int atemptStart = FALSE;       //inicio de transmissao
int atemptCount = 0;           //tentativas de transmissão
int state = 0;                 //estado da máquina de estado
int type = 0;                  //tipo de trama
int checksum = 0;              //???????????????''
int fd;                        //????????????????
int N=0;                       //Ns = 0 || 1
time_t total_time;             //tempo total do programa -> resolução de 1s
time_t file_time;              //tempo total de transferencia do ficheiro -> resolução de 1s

int debug_i = 0;               //debug only

//Debug only
void debugp(int s)
{
    printf("Debug nº %d -> %d\n",debug_i, s);
    debug_i++;
}

void protocol_stats(int r)
{
  total_time = time(NULL) - total_time;

  pd.num_set_b = atemptCount;

  //como fazer para o pd.num_i_b e pd.num_disc_Tb??????
  //faz se reset ao atemptCount e poe se o pd.num_set_b=atemptCount dentro do llopen

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
  // printf("Error Recovery\n"); //Error recovery for example, close the connection (DISC) re establish it (SET) and restart the process;
  // printf("%d\n", 1);
  // printf("Event Log\n");//-Event log (errors);
  // printf("%d\n", 1);
  printf("Time of transfer of file data -> %ld s\n", file_time);
  printf("Total Time of Program -> %ld s\n", total_time);
}

//o erro do invalid argument era devido a pormos como int em vez de Bint
//testar no lab para ver quais os valores de oldtio e ver se usa se o oldtio ou o baudrate do main.c
//tem que retornar um int
//case n é em principio um int
int baudrate_check(int baudrate_i)
{
  int baudrate_f;
  switch (baudrate_i)
  {
    case 1200: case 9:
      baudrate_f = B1200;//9
      break;
    case 1800: case 10:
      baudrate_f = B1800;//10
      break;
    case 2400: case 11:
      baudrate_f = B2400;//11
      break;
    case 4800: case 12:
      baudrate_f = B4800;//12
      break;
    case 9600: case 13:
      baudrate_f = B9600;//13
      break;
    case 19200: case 14:
      baudrate_f = B19200;//14
      break;
    case 38400: case 15:
      baudrate_f = B38400;//15
      break;
    case 57600: case 4097:
      baudrate_f = B57600;//4097
      break;
    case 115200: case 4098:
      baudrate_f = B115200;//4098
      break;
    default:
      baudrate_f = BAUDRATE_DEFAULT;//15
      break;
  }
  return baudrate_f;
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
    //STOP = FALSE; // se não ele não entra no ciclo while de novo
    checksum = 0;
    if (atemptCount > ll->numTries)
    {
        printf("Dropping Connection\n");
    }
    else
    {
        pd.tries++;
        printf("Atempt #%d\n", atemptCount);
    }
}

void errorcheck(int s)
{
    printf("An error ocurred in byte nº %d\n", state);
    checksum = -1; // error
}

//Máquina de Estados
void statemachine(unsigned char buf, int type)
{
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
          //checksum = 0;
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
          //checksum = 0;
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
    total_time = time(NULL);

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

    printf("%02X \n", cfgetospeed(&oldtio));
    printf("%d \n", cfgetospeed(&oldtio));

    // Clear struct for new port settings
    bzero(&newtio, sizeof(newtio));

    newtio.c_cflag = baudrate_check(cfgetospeed(&oldtio)) | CS8 | CLOCAL | CREAD; //ESTE EM principio SERA O FINAL E CORRETO

    //newtio.c_cflag = baudrate_check(ll->baudRate) | CS8 | CLOCAL | CREAD;

    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;
    newtio.c_lflag = 0;

    if(ll->role == TRANSMITTER)
    {
      newtio.c_cc[VTIME] = 30; // Inter-character timer unused
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

    //Init Stats
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
        while((state != 5) && (atemptCount < (ll->numTries+1)))
        {
            if (atemptStart == FALSE)
            {
                // Create frame to send
                unsigned char buf_sent[TYPE1_SIZE] = {FLAG, A, C1, BCC1, FLAG};

                for (int i = 0; i < TYPE1_SIZE; i++)
                {
                    printf("%02X ", buf_sent[i]);
                }

                STOP = FALSE;
                int bytes_sent = write(fd, buf_sent, TYPE1_SIZE);

                alarm(ll->timeOut);  // Set alarm to be triggered in 3s
                atemptStart = TRUE;

                printf("\nSET Frame Sent\n");
                pd.num_set_a++;
                printf("%d bytes written\n", bytes_sent);
                printf("Waiting for UA\n");

                while (STOP == FALSE)
                {
                    unsigned char buf_read[TYPE1_SIZE];
                    int bytes_read = read(fd, buf_read, TYPE1_SIZE);
                    int buf_size = TYPE1_SIZE;
                    if (bytes_read != TYPE1_SIZE)
                    {
                      buf_size = 0;
                    }

                    for (int i = 0; i < buf_size; i++)
                    {
                        if(checksum != -1)
                        {
                          printf("%02X ", buf_read[i]);
                          statemachine(buf_read[i],type);
                        }
                    }

                    if (checksum == 2)
                    {
                        printf("%d bytes received\n", bytes_read);
                        printf("UA Frame Received Sucessfully\n");
                        pd.num_ua_a++;
                        alarm(0); //desativa o alarme
                        printf("Ending tx setup\n");
                        return_check = 1;
                        file_time = time(NULL);
                    }
                    STOP = TRUE;
                }
            }
        // codigo fica aqui preso a espera do 3s do alarme
        }
    }
    else if(ll->role == RECEIVER)
    {
        unsigned char buf_read[TYPE1_SIZE];
        while (STOP == FALSE)
        {
            // Returns after 5 chars have been input
            int bytes_read = read(fd, buf_read, TYPE1_SIZE);

            for (int i = 0; i < TYPE1_SIZE; i++)
            {
              printf("%02X ", buf_read[i]);
              statemachine(buf_read[i],type);
            }

            printf("%d bytes received\n", bytes_read);

            if (checksum == 2)
            {
                printf("SET Frame Received Sucessfully\n");
                pd.num_set_a++;
                unsigned char buf_sent[TYPE1_SIZE] = {FLAG, A, C2, BCC2, FLAG};

                for (int i = 0; i < TYPE1_SIZE; i++)
                {
                  printf("%02X ", buf_sent[i]);
                }

                int bytes_sent = write(fd, buf_sent, TYPE1_SIZE);
                printf("\nUA Frame Sent\n");
                pd.num_ua_a++;
                printf("%d bytes written\n", bytes_sent);
                printf("Ending rx setup\n");
                STOP = TRUE;
                return_check = 1;
                file_time = time(NULL);
            }
        }
    }
    state = 0;          //reset da maquina de estados
    STOP = FALSE;       //reset
    atemptStart = FALSE;//reset
    return return_check;
}

// Send data in buf with size bufSize.
// Return number of chars written, or "-1" on error.
int llwrite(char *buf, int bufSize)
{
    return_check = -1;
    type = 1;
    int data_s = 0;

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
    framesize = 4;
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
        frame[framesize] = 0x5e;
      }
      framesize++;
      data_s++;
    }

    //notsure
    frame[framesize] = check_bcc2(buf,bufSize);
    if(stuffing(frame[framesize]))
    {
      frame[framesize] = 0x7d;
      framesize++;
      frame[framesize] = 0x5e;
    }
    framesize++;
    frame[framesize] = FLAG;
    framesize++;

    while((state != 5) && (atemptCount < (ll->numTries+1)))
    {
      if (atemptStart == FALSE)
      {
        STOP = FALSE;
        write(fd, frame, framesize);

        alarm(ll->timeOut);  // Set alarm to be triggered in 3s
        atemptStart = TRUE;

        printf("\nI Frame Sent\n");
        pd.num_i_a++;

        // printf("%d bytes written\n",return_check);
        printf("Waiting for RR\n");

        while (STOP == FALSE)
        {
          int bytes_read = read(fd, buf, TYPE1_SIZE);
          int buf_size = TYPE1_SIZE;
          if (bytes_read != TYPE1_SIZE)
            {
              buf_size = 0;
            }

          for (int i = 0; i < buf_size; i++)
          {
            if(checksum != -1)
            {
              statemachine(buf[i],type);
            }
          }

          if (checksum == 2)
          {
            printf("%d bytes received\n", bytes_read);
            printf("RR Frame Received Sucessfully\n");
            pd.num_rr++;
            alarm(0); //desativa o alarme
            return_check = data_s;
          }
          STOP = TRUE;
        }
      }
    // codigo fica aqui preso a espera do 3s do alarme
    }

    state = 0;
    atemptStart = FALSE;
    STOP = FALSE;
    printf("Return check last: %d \n",return_check);
    return return_check;
}

// Receive data in packet.
// Return number of chars read, or "-1" on error.
int llread(char *packet)
{
    char frame_data[MAX_PAYLOAD_SIZE + 8];
    return_check = -1;
    type = 2;
    int bytes_read = 0;
    int check = 0;
    char tmp_buf[1];
    int bcc2_pos = 0;
    int destf_count = 0;
    int datasize = 0;

    while (STOP == FALSE)
    {
      while (check != 2)
      {
        read(fd,tmp_buf,1);
        if(tmp_buf[0] == FLAG)
        {
          frame_data[bytes_read] = tmp_buf[0];
          check++;
          if(check == 2)
          {
            break;
          }
        }
        else
        {
          frame_data[bytes_read] = tmp_buf[0];
        }
        bytes_read += 1;
      }

      //stuffing do bcc2 e verficação da posição do bcc2 para colocar os dados só até ao bcc2(não inclusive)
      char bcc2;
      int bcc2p;
      bcc2_pos=bytes_read;
      if(destuffing(frame_data[bytes_read-2],frame_data[bytes_read-1]))
      {
        destf_count++;
        bcc2_pos -= 2;
        bcc2 = FLAG;
      }
      else
      {
        bcc2_pos -= 1;
        //printf("bcc2 not stuffed %02X\n",frame_data[bcc2_pos]);
        bcc2 = frame_data[bcc2_pos];
      }
      bcc2p = bcc2_pos;

      //destuffing e colocação dos dados no pacote
      for (int i = 0; i < bytes_read + 1; i++)//ir até flag bytesread=pos de flag
      {
        statemachine(frame_data[i],type);
        if((state == 4) && (i > 3) && (i < bcc2p))
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
            packet[datasize] = frame_data[i];
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
        STOP = TRUE;
        checksum = -1;
      }
      // printf("Checksum: %d \n",checksum);
      if (checksum == 2)
      {
        printf("I Frame Received Sucessfully\n");
        pd.num_rr++;
        unsigned char buf_sent[TYPE1_SIZE];
        buf_sent[0] = FLAG;
        buf_sent[1] = A;
        if (!N)
        {
          buf_sent[2] = C_RR0;
          buf_sent[3] = BCC_RR0;
        }
        else
        {
          buf_sent[2] = C_RR1;
          buf_sent[3] = BCC_RR1;
        }
        buf_sent[4] = FLAG;

        for (int i = 0; i < TYPE1_SIZE; i++)
        {
          printf("%02X ", buf_sent[i]);
        }

        int bytes_sent = write(fd, buf_sent, TYPE1_SIZE);
        printf("\nRR Frame Sent\n");
        pd.num_i_a++;
        printf("%d bytes written\n", bytes_sent);
        STOP = TRUE;
      }
    }

    datasize = 0;
    state = 0;
    STOP = FALSE;
    printf("Return check last: %d \n",return_check);
    return return_check;
}

// Close previously opened connection.
// if showStatistics == TRUE, link layer should print statistics in the console on close.
// Return "1" on success or "-1" on error.
int llclose(int showStatistics)
{
    file_time = time(NULL) - file_time;

    return_check = -1;
    type = 1;

    if(ll->role == TRANSMITTER)
    {
        do
        {
            if (atemptStart == FALSE)
            {
                // Create frame to send
                unsigned char buf_sent[TYPE1_SIZE] = {FLAG, A, C_DISC, BCC_DISC, FLAG};
                unsigned char buf_read[TYPE1_SIZE];

                for (int i = 0; i < TYPE1_SIZE; i++)
                {
                    printf("%02X ", buf_sent[i]);
                }

                STOP = FALSE;
                int bytes_sent = write(fd, buf_sent, TYPE1_SIZE);

                alarm(ll->timeOut);  // Set alarm to be triggered in 3s
                atemptStart = TRUE;

                printf("\n1st DISC Frame Sent\n");
                pd.num_disc_T++;
                printf("%d bytes written\n", bytes_sent);
                printf("Waiting for 2nd DISC\n");

                while (STOP == FALSE)
                {
                    int bytes_read = read(fd, buf_read, TYPE1_SIZE);
                    int buf_size = TYPE1_SIZE;
                    if (bytes_read != TYPE1_SIZE)
                    {
                        buf_size = 0;
                    }

                    for (int i = 0; i < buf_size; i++)
                    {
                        if(checksum != -1)
                        {
                          printf("%02X ", buf_read[i]);
                          statemachine(buf_read[i],type);
                        }
                    }

                    if (checksum == 2)
                    {
                        printf("%d bytes received\n", bytes_read);
                        printf("2nd DISC Frame Received Sucessfully\n");
                        pd.num_disc_R++;
                        alarm(0); //desativa o alarme
                    }
                    STOP = TRUE;
                }
                atemptStart=FALSE;
            }
            if (atemptStart == FALSE)//bug????? n deveria ser fora e dentro de um novo while
            {
              unsigned char buf_sent[TYPE1_SIZE] = {FLAG, A, C2, BCC2, FLAG};
              for (int i = 0; i < TYPE1_SIZE; i++)
              {
                  printf("%02X ", buf_sent[i]);
              }

              int bytes_sent = write(fd, buf_sent, TYPE1_SIZE);

              //atemptStart = TRUE;

              printf("\nUA Frame Sent\n");
              pd.num_ua_b++;
              printf("%d bytes written\n", bytes_sent);
              return_check = 1;
            }
        // codigo fica aqui preso a espera do 3s do alarme
      }while((state != 5) && (atemptCount < (ll->numTries+1)));
    }
    else if(ll->role == RECEIVER)
    {
        unsigned char buf_read[TYPE1_SIZE];
        while (STOP == FALSE)
        {
            // Returns after 5 chars have been input
            int bytes_read = read(fd, buf_read, TYPE1_SIZE);

            for (int i = 0; i < TYPE1_SIZE; i++)
            {
              printf("%02X ", buf_read[i]);
              statemachine(buf_read[i],type);
            }

            printf("%d bytes received\n", bytes_read);

            if (checksum == 2)
            {
                printf("1st DISC Frame Received Sucessfully\n");
                pd.num_disc_R++;
                unsigned char buf_sent[TYPE1_SIZE] = {FLAG, A, C_DISC, BCC_DISC, FLAG};

                for (int i = 0; i < TYPE1_SIZE; i++)
                {
                  printf("%02X ", buf_sent[i]);
                }

                int bytes_sent = write(fd, buf_sent, TYPE1_SIZE);
                printf("\n2nd DISC Frame Sent\n");
                pd.num_disc_T++;
                printf("%d bytes written\n", bytes_sent);
                printf("Waiting for UA\n"); //CASO N RECEBA FAZ DISC NA MSM SE N ME ENGANO CONGERIR CADERNO
                STOP = TRUE;
            }

            // for (int i = 0; i < TYPE1_SIZE; i++)
            // {
            //   printf("%02X ", buf_read[i]);
            // }

            memset(buf_read, 0, sizeof buf_read);

            // for (int i = 0; i < TYPE1_SIZE; i++)
            // {
            //   printf("%02X ", buf_read[i]);
            // }

            //unsigned char buf_read2[TYPE1_SIZE];
            //ele consegue fzr override do buf_read antigo é suposto?
            //de qualquer modo o memset vai dar clear ao buff_read
            bytes_read = read(fd, buf_read, TYPE1_SIZE);

            for (int i = 0; i < TYPE1_SIZE; i++)
            {
              printf("%02X ", buf_read[i]);
              statemachine(buf_read[i],type);
            }

            printf("%d bytes received\n", bytes_read);

            if (checksum == 2)
            {
                printf("UA Frame Received Sucessfully\n");
                pd.num_ua_b++;
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
