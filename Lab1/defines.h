
//em vez de usar hexa se calhar é mais correto usar binario?

//0b01111110 -> 7E
#define FLAG 0X7E
//0b00000101 -> 5 Transmitter/Receiver
#define A 0X05
//0b00000011 -> 3 SET RX
#define C1 0X03
//XOR(A,C) -> 6
#define BCC1 (A^C1)
//0b00000111 -> 7 UA TX
#define C2 0X07
//XOR(A,C) -> 2
#define BCC2 (A^C2)
//Supervision and Unnumbered Frames size
#define TYPE1_SIZE 5