#include "linklayer.h"
#include "defines.h"           //ficheiro com os defines

#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>
#include <stdbool.h>           // library for the use of bool
#include <time.h>              // library for the use of time_t

linkLayer *ll;                 // struct with information and serial port parameters
protocol_data pd;              // struct with statistical information of the program
struct termios oldtio;         // struct termios for serial port control

volatile int STOP = FALSE;     // reception start
int return_check = -1;         // -1 if fail || 1 if success for llclose & llopen || data for llwrite & llread
int attemptStart = FALSE;      // transmission attempt
int attemptCount = 0;          // transmission attempts
int state = 0;                 // state of the state machine
int type = 0;                  // type of frame -> 1 for Supervision and Unnumbered frames || 2 for Information frames
int checksum = 0;              // 1 if received C || 2 if received 2nd FLAG || -1 if error
int fd;                        // file
int N = 0;                     // Ns = 0 || 1
time_t total_time;             // total program time -> resolution of 1s
time_t file_time;              // total file transfer time -> resolution of 1s

int debug_i = 0;               // debug APAGAR

// Debug function APAGAR
void debugp(int s)
{
  printf("Debug nº %d -> %d\n",debug_i, s);
  debug_i++;
}

// Prints the file statistics from the Transmitter side or Receiver side accordingly
void protocol_stats(int r)
{
  total_time = time(NULL) - total_time;

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
  printf("Time of transfer of file data -> %ld s\n", file_time);
  printf("Total Time of Program -> %ld s\n", total_time);
}

// Checks the current baudrate defined in the termios struct and adjusts it according to standard termios values
int baudrate_check(int baudrate_i)
{
  int baudrate_f;
  switch (baudrate_i)
  {
    case 1200: case 9:
      baudrate_f = B1200;//9 in decimal
      break;
    case 1800: case 10:
      baudrate_f = B1800;//10 in decimal
      break;
    case 2400: case 11:
      baudrate_f = B2400;//11 in decimal
      break;
    case 4800: case 12:
      baudrate_f = B4800;//12 in decimal
      break;
    case 9600: case 13:
      baudrate_f = B9600;//13 in decimal
      break;
    case 19200: case 14:
      baudrate_f = B19200;//14 in decimal
      break;
    case 38400: case 15:
      baudrate_f = B38400;//15 in decimal
      break;
    case 57600: case 4097:
      baudrate_f = B57600;//4097 in decimal
      break;
    case 115200: case 4098:
      baudrate_f = B115200;//4098 in decimal
      break;
    default:
      baudrate_f = BAUDRATE_DEFAULT;//15 in decimal
      break;
  }
  return baudrate_f;
}

//??????????????????
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

//??????????????????????
//se aparecer 0x7e/FLAG/01111110 é modificado pela sequencia 0x7d0x5e(0x7d/1111101-0x5e/1011110)
//ou escape octate + resultado do ou exclusivo de 0x7e com 0x20
bool destuffing(char buf1,char buf2)
{
  return ((buf1 == 0x7d) && (buf2 == 0x5e));
}

//???????????????????????''
//obj: se a FLAG aparecer em A OU C fazer stuffing e retornar true caso ocorra stuffing e false caso contrario
//se aparecer 0x7e/FLAG/01111110 é modificado pela sequencia 0x7d0x5e(0x7d/1111101-0x5e/1011110)
//ou escape octate + resultado do ou exclusivo de 0x7e com 0x20
//unsigned char stuffing (unsigned char* buf,int buf_size)
bool stuffing (char buf)
{
  return buf == FLAG;
}

// Handler that will be triggered with the alarm
void atemptHandler(int signal)
{
  attemptCount++;
  attemptStart = FALSE; //se não ele não entra no ciclo while de novo
  checksum = 0;
  if (attemptCount > ll->numTries)
  {
    printf("Dropping Connection\n");
  }
  else
  {
    pd.tries++;
    printf("Atempt #%d\n", attemptCount);
  }
}

// Updates checksum incase an error ocorred in the state machine
void errorcheck()
{
  checksum = -1; // error
}

// State Machine that analyses the frames
void statemachine(unsigned char buf, int type)
{
  switch (state)
  {
    case 0: //Start
      if (buf == FLAG)
      {
        state = 1;
        checksum = 0;
      }
      else
      {
        errorcheck();
      }
      break;
    case 1: //FLAG ok
      if (buf == A)
      {
        state = 2;
      }
      else if (buf == FLAG)
      {
        state = 1;
      }
      else
      {
        errorcheck();
      }
      break;
    case 2: //A ok
      if ((buf == C2))
      {
        state = 3;
        checksum++;
      }
      else if ((buf == C1) && (ll->role == RECEIVER))
      {
        state = 3;
        checksum++;
      }
      else if ((buf == (C_I0))||(buf == (C_RR0)) || (buf == (C_REJ1)))
      {
        N = 1;
        state = 3;
        checksum++;
      }
      else if ((buf == (C_I1))||(buf == (C_RR1)) || (buf == (C_REJ0)))
      {
        N = 0;
        state = 3;
        checksum++;
      }
      else if(buf == C_DISC)
      {
        state = 3;
        checksum++;
      }
      else if (buf == FLAG)
      {
        state = 1;
      }
      else
      {
        errorcheck();
      }
      break;
    case 3: //C ok
      if ((buf == (BCC2)))
      {
        state = 4;
      }
      else if ((buf == (BCC1)))
      {
        state = 4;
      }
      else if ((buf == (BCC_I0))||(buf == (BCC_RR0))||(buf == (BCC_REJ1)))
      {
        N = 1;
        state = 4;
      }
      else if ((buf == (BCC_I1))||(buf == (BCC_RR1))||(buf == (BCC_REJ0)))
      {
        N = 0;
        state = 4;
      }
      else if ((buf == (BCC_DISC)))
      {
        state = 4;
      }
      else if (buf == FLAG)
      {
        state = 1;
      }
      else
      {
        errorcheck();
      }
      break;
    case 4: //BCC ok
      if (type == 1)
      {
        if (buf == FLAG)
        {
          state = 5;
          checksum++;
        }
        else
        {
          errorcheck();
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
    case 5: //FLAG ok End
      break;
  }
}

// Open a connection using the "port" parameters defined in struct linkLayer.
// Return "1" on success or "-1" on error.
int llopen(linkLayer connectionParameters)
{
  total_time = time(NULL);

  // Initialize parameters of the linkLayer struct ll with the parameters sent by the application
  ll = (linkLayer *) malloc(sizeof(linkLayer));
  ll->baudRate = connectionParameters.baudRate;
  ll->role = connectionParameters.role;
  ll->numTries = connectionParameters.numTries;
  ll->timeOut = connectionParameters.timeOut;
  sprintf(ll->serialPort,"%s",connectionParameters.serialPort);

  // Open file???
  fd = open(ll->serialPort, O_RDWR | O_NOCTTY );
  if (fd < 0)
  {
    perror(ll->serialPort);
    exit(-1);
  }
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

  //  Check baudrate for the new termios struct
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

  // initialize protocol_data struct pd
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
  pd.tries = 1;             // 1 in order to account for the initial frame that will cause the retransmission

  type = 1;

  if(ll->role == TRANSMITTER)
  {
    while((state != 5) && (attemptCount < (ll->numTries+1)))
    {
      if (attemptStart == FALSE)
      {
        // Create SET frame to send
        unsigned char buf_sent[TYPE1_SIZE] = {FLAG, A, C1, BCC1, FLAG};

        for (int i = 0; i < TYPE1_SIZE; i++)//fica o print?
        {
          printf("%02X ", buf_sent[i]);
        }

        STOP = FALSE;
        int bytes_sent = write(fd, buf_sent, TYPE1_SIZE);

        alarm(ll->timeOut);  // Set alarm to be triggered in 3s
        attemptStart = TRUE;

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
            alarm(0); // turns off alarm
            printf("Ending tx setup\n");
            return_check = 1;
            file_time = time(NULL);
            pd.num_set_b = attemptCount;
            attemptCount = 0;
          }
          STOP = TRUE;
        }
      }
    // 3s alarm
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
        // Create UA frame to respond
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
  state = 0;
  STOP = FALSE;
  attemptStart = FALSE;
  return return_check;
}

// Send data in buf with size bufSize.
// Return number of chars written, or "-1" on error.
int llwrite(char *buf, int bufSize)
{
  return_check = -1;
  type = 1;
  int data_s = 0;

  // Create I frame to send
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

  while((state != 5) && (attemptCount < (ll->numTries+1)))
  {
    if (attemptStart == FALSE)
    {
      STOP = FALSE;
      write(fd, frame, framesize);

      alarm(ll->timeOut);  // Set alarm to be triggered in 3s
      attemptStart = TRUE;

      printf("\nI Frame Sent\n");
      pd.num_i_a++;
      // pd.num_i_b = attemptCount;   //caso ele faca timeout no llwrite

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
          if(((buf[2] == C_REJ0) && (buf[3] == BCC_REJ0))|| ((buf[2] == C_REJ1) && (buf[3] == BCC_REJ1)))
          {
            printf("REJ received\n");
            checksum = 0;
            pd.num_rej++;
            pd.num_i_b++;
            state = 0;
            alarm(0); // turns off alarm
            attemptStart = FALSE;
          }
          else
          {
            printf("%d bytes received\n", bytes_read);
            printf("RR Frame Received Sucessfully\n");
            pd.num_rr++;
            alarm(0); // turns off alarm
            return_check = data_s;  // number of chars written
            pd.num_i_b += attemptCount;
            attemptCount = 0;
          }
        }
        // else                       //n funciona pois vai ultrapassar em mais +\ o dps se tiver on ele vai somar +=
        // {
        //   printf("HELLO1 %d\n",pd.num_i_b);
        //   pd.num_i_b = attemptCount;
        //   printf("HELLO2 %d\n",pd.num_i_b);
        // }
        STOP = TRUE;
      }
    }
  // 3s alarm
  }
  printf("HELLO3 %d\n",pd.num_i_b);
  state = 0;
  attemptStart = FALSE;
  STOP = FALSE;

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

  for(int i = 0; i < 4; i++)
  {
    read(fd,tmp_buf,1);
    frame_data[bytes_read] = tmp_buf[0];
    statemachine(frame_data[bytes_read],type);
    bytes_read += 1;
    if(checksum == -1)
    {
      STOP = TRUE;
      return_check = 0;
      break;
    }
  }
  checksum = 0;
  state = 0;

  while (STOP == FALSE)
  {
    while (check != 2)
    {
      read(fd,tmp_buf,1);

      if(tmp_buf[0] == FLAG && bytes_read != 0)
      {
        frame_data[bytes_read] = tmp_buf[0];
        check = 2;
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
          packet[datasize] = FLAG;
          i++;
          datasize++;
        }
        else
        {
          packet[datasize] = frame_data[i];
          datasize++;
        }
      }
    }
    return_check = datasize; // number of chars read

    if(bcc2 != check_bcc2(packet,datasize))
    {
      printf("bcc2 original: %02X \n",bcc2);
      printf("bcc2 calculado2: %02X \n",check_bcc2(packet,datasize));

      pd.num_rej++;
      // Create REJ frame to respond
      unsigned char buf_sent[TYPE1_SIZE];
      buf_sent[0] = FLAG;
      buf_sent[1] = A;
      if (!N)
      {
        buf_sent[2] = C_REJ1;
        buf_sent[3] = BCC_REJ1;
      }
      else
      {
        buf_sent[2] = C_REJ0;
        buf_sent[3] = BCC_REJ0;
      }
      buf_sent[4] = FLAG;

      for (int i = 0; i < TYPE1_SIZE; i++)
      {
        printf("%02X ", buf_sent[i]);
      }

      int bytes_sent = write(fd, buf_sent, TYPE1_SIZE);
      printf("\n REJ Sent\n");
      pd.num_i_a++;
      printf("%d bytes written\n", bytes_sent);

      STOP = TRUE;
      return_check = 0; //dados para o ficheiro rejeitados
      checksum = -1; //mudar para errorcheck?????
    }

    if (checksum == 2)
    {
      printf("I Frame Received Sucessfully\n");
      pd.num_rr++;
      // Create RR frame to send
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
    while((state != 5) && (attemptCount < (ll->numTries+1)))
    {
      if (attemptStart == FALSE)
      {
        // Create DISC frame to send
        unsigned char buf_sent[TYPE1_SIZE] = {FLAG, A, C_DISC, BCC_DISC, FLAG};
        unsigned char buf_read[TYPE1_SIZE];

        for (int i = 0; i < TYPE1_SIZE; i++)
        {
          printf("%02X ", buf_sent[i]);
        }

        STOP = FALSE;
        int bytes_sent = write(fd, buf_sent, TYPE1_SIZE);

        alarm(ll->timeOut);  // Set alarm to be triggered in 3s
        attemptStart = TRUE;

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
            alarm(0); // turns off alarm
            attemptCount = 0;
          }
          // else       //caso haja timeout no disc
          // {
          //   printf("HELLO1 %d\n",pd.num_disc_Tb);
          //   pd.num_disc_Tb = attemptCount;
          //   printf("HELLO2 %d\n",pd.num_disc_Tb);
          // }
          STOP = TRUE;
        }
      }
    // 3s alarm
    }

    if(attemptCount < (ll->numTries+1))
    {
      // Create UA frame to send
      unsigned char buf_sent[TYPE1_SIZE] = {FLAG, A, C2, BCC2, FLAG};
      for (int i = 0; i < TYPE1_SIZE; i++)
      {
        printf("%02X ", buf_sent[i]);
      }

      int bytes_sent = write(fd, buf_sent, TYPE1_SIZE);

      printf("\nUA Frame Sent\n");
      pd.num_ua_b++;
      printf("%d bytes written\n", bytes_sent);
      return_check = 1;
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
        printf("1st DISC Frame Received Sucessfully\n");
        pd.num_disc_R++;
        // Create DISC frame to respond
        unsigned char buf_sent[TYPE1_SIZE] = {FLAG, A, C_DISC, BCC_DISC, FLAG};

        for (int i = 0; i < TYPE1_SIZE; i++)
        {
          printf("%02X ", buf_sent[i]);
        }

        int bytes_sent = write(fd, buf_sent, TYPE1_SIZE);
        printf("\n2nd DISC Frame Sent\n");
        pd.num_disc_T++;
        printf("%d bytes written\n", bytes_sent);
        printf("Waiting for UA\n");
        STOP = TRUE;
      }
      memset(buf_read, 0, sizeof buf_read); //empties buf_read of previous data
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

  //Restore the old port settings
  if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
  {
    perror("tcsetattr");
    exit(-1);
  }

  close(fd);

  if (showStatistics)
  {
    protocol_stats(ll->role);
  }

  return return_check;
}
