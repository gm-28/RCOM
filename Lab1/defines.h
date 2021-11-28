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

//0bX -> X I TX
#define C_I 0X00
//XOR(A,C) -> X
#define BCC_I (A^C_I)

//0bX -> X RR RX
#define C_RR 0X00
//XOR(A,C) -> X
#define BCC_RR (A^C_RR)
