Repositório código Lab1

TO-DO:

  -Quickly move to the cable program console and press 0 for unplugging the cable, 2 to add noise, and 1 to normal
	5.3. Should have received a nice looking penguin even if the cable disconnected or with noise added???????????'
  
  -caso haja erro dps do read (llread) e dps do write (llwrite), apesar destes terem tido sucesso temos que retransmitir usando o timeout
  
  -REJ só em problemas de dados e de BCC2
  
Valorisation Elements:
   
   -REJ implementation;
   
   File transmission statistics:
       -Error recovery for example, close the connection (DISC) re establish it (SET) and restart the process;
       -Event log (errors);
       -Number of retransmitted/received I frames, number of time outs, number of sent/received REJ;
       
   -slide 26
   
   -byte stuffing 7d

Notas:
  
  -ter em atençao a quando llwrite e llread retornam, não podemos ter dessincronização
  
