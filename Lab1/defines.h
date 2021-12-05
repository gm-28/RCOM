//mudar dps nomenclatura dos defines BCC1 -> BCC_TX

typedef struct protocol_data {
  int num_set_a;  // total number of SET frames sent
  int num_i_a;    // total number of I frames sent
  int num_disc_T; // total number of DISC frames sent
  int num_ua_b;   // total number of UA frames sent

  int num_ua_a;   // total number of UA frames received
  int num_rr;     // total number of RR frames received
  int num_rej;    // total number of REJ frames received
  int num_disc_R; // total number of DISC frames received

  int num_set_b;  // total number of SET frames retransmitted
  int num_i_b;    // total number of I frames retransmitted
  int num_disc_Tb;// total number of DISC frames retransmitted

  int tries;      // total number of timeouts
} protocol_data;

// Supervision and Unnumbered Frames size -> 5
#define TYPE1_SIZE 5

// 0b01111110 -> 7E
#define FLAG 0X7E
// 0b00000101 -> 5
#define A 0X05

// 0b00000011 -> 3 SET
#define C_SET 0X03
// XOR(A,C) -> 6
#define BCC_SET (A^C1)

// 0b00000111 -> 7 UA
#define C_UA 0X07
// XOR(A,C) -> 2
#define BCC_UA (A^C2)

// 0b00000000 -> 0 I
#define C_I0 0X00
// XOR(A,C_I0) -> 5
#define BCC_I0 (A^C_I0)
// 0b00000010 -> 2 I
#define C_I1 0X02
//XOR(A,C_I1) -> 7
#define BCC_I1 (A^C_I1)

// 0b00000001 -> 1 RR
#define C_RR0 0X01
// XOR(A,C_RR0) -> 4
#define BCC_RR0 (A^C_RR0)
// 0b00100001 -> 21 RR
#define C_RR1 0X21
// XOR(A,C_RR1) -> 24
#define BCC_RR1 (A^C_RR1)

// 0b00000101 -> 5 REJ
#define C_REJ0 0X05
// XOR(A,C_REJ0) -> 0
#define BCC_REJ0 (A^C_REJ0)
// 0b00100101 -> 25 REJ
#define C_REJ1 0X25
// XOR(A,C_REJ1) -> 20
#define BCC_REJ1 (A^C_REJ1)

// 0b00001011 -> B DISC
#define C_DISC 0X0B
// XOR(A,C_DISC) -> E
#define BCC_DISC (A^C_DISC)
