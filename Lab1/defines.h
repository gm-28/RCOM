//mudar dps nomenclatura dos defines BCC1 -> BCC_TX
//Supervision and Unnumbered Frames size -> 5
#define TYPE1_SIZE 5

//0b01111110 -> 7E
#define FLAG 0X7E
//0b00000101 -> 5 Transmitter/Receiver
#define A 0X05

//0b00000011 -> 3 SET TX
#define C1 0X03
//XOR(A,C) -> 6
#define BCC1 (A^C1)

//0b00000111 -> 7 UA RX
#define C2 0X07
//XOR(A,C) -> 2
#define BCC2 (A^C2)

//0b00000000 -> 0 I TX
#define C_I0 0X00
//XOR(A,C_I0) -> 5
#define BCC_I0 (A^C_I0)
//0b00000010 -> 2 I TX
#define C_I1 0X02
//XOR(A,C_I1) -> 7
#define BCC_I1 (A^C_I1)

//0b00000001 -> 1 RR RX
#define C_RR0 0X01
//XOR(A,C_RR0) -> 4
#define BCC_RR0 (A^C_RR0)
//0b00100001 -> 21 RR RX
#define C_RR1 0X21
//XOR(A,C_RR1) -> 24
#define BCC_RR1 (A^C_RR1)

//0b00001011 -> B DISC
#define C_DISC 0X0B
//XOR(A,C_DISC) -> E
#define BCC_DISC (A^C_DISC)
