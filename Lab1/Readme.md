Repositório código Lab1

ERRO:
 
 -Erro se n receber nd no llopen P.S. alarms tao todos comentados para ja

TO-DO:
 
  -caso haja erros?
  
  -caso haja erro dps do read (llread) e dps do write (llwrite), apesar destes terem tido sucesso temos que retransmitir usando o timeout
  
  -REJ só em problemas de dados e de BCC2
  
  -Switch com baud rate
  
Valorisation Elements:

   -Random error generation in data frames: Suggestion for each correctly received I frame, simulate at the receiver the occurence of errors in header and data field according to     pre defined and independent probabilities, and proceed as if they were real errors;
   
   -REJ implementation;
   
   File transmission statistics:
       -Error recovery for example, close the connection (DISC) re establish it (SET) and restart the process;
       -Event log (errors);
       -Number of retransmitted/received I frames, number of time outs, number of sent/received REJ;
       
   -slide 26
   
   -byte stuffing 7d

Notas:
  
  -ter em atençao a quando llwrite e llread retornam, não podemos ter dessincronização
  
Dúvidas:
  
  -falar stor -> com noise ocorre problema semelhante ao da aula
  
