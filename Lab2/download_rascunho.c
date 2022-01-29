#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#define SERVER_PORT 21
#define MAX_SIZE 1024

struct sockaddr_in server_addr;
struct hostent *h; 
char enter[MAX_SIZE] = "\n";
char user_command[MAX_SIZE] = "user ";
char pass_command[MAX_SIZE] = "pass ";
char *host;
char *url_path;
char *filename;
int flag = 0;

int open_socket(int port){
    bzero((char*)&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(inet_ntoa(*((struct in_addr *)h->h_addr)));  // 32 bit Internet address network byte ordered
    server_addr.sin_port = htons(port);  // Server TCP port must be network byte ordered

    // Open a TCP socket
    int socketfd = socket(AF_INET, SOCK_STREAM, 0);

    if (socketfd < 0)
    {
        perror("socket()");
        exit(-1);
    }

    // Connect to the server
    if (connect(socketfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("connect()");
        exit(-1);
    }
    return socketfd;
}

int read_from_socket(FILE *file,int command_socket, char *response){
    while(response[0] != '5')
    {
        memset(response, 0, MAX_SIZE);
        fgets(response, MAX_SIZE, file);
        
        if(response[0]!='3')
            printf("%s",response);

        if(response[3] == ' ' || response[0] == '3' )
            break;
    }

    if(response[0] == '5' ){
        return -1;
    }

    return 1;
}

int write_to_socket(int command_socket,char *command){
    int bytesWritten = 0;
    if((bytesWritten = write(command_socket,command,strlen(command))) < strlen(command))
    {
        perror("Error writing command to socket\n");
        return -1;
    }
      return bytesWritten;
}

/*parsing of the url assuming the ftp connection standard
in every case the host, the url_path and the filename are obtained
in case the user inserts username and password, the host variable will contain that information so we parse it as well
*/
void url_parsing(char *url){
    char *pass;
    char *user;
    
    strtok(url, "//");
    host = strtok(NULL, "/");
    url_path = strtok(NULL, "");
    filename = strrchr(url_path, '/');

    char *aux_login = host;
    char *aux_host = strrchr(aux_login, '@');;
    
    if (aux_host != NULL)
    {
        host = aux_host+1;
        user = strtok(aux_login, ":");
        pass = strtok(NULL, "@");
        
        printf("User: %s\n", user);
        printf("Pass: %s\n", pass);
        
        strcat(user_command,user);
        strcat(user_command,enter);
        strcat(pass_command,pass);
        strcat(pass_command,enter);

        flag = 1;
    }
}

int port_calc(char *c5, char *c6)
{
    int x5 = atoi(c5);
    int x6 = atoi(c6);
    //printf("%d %d\n", x5, x6);

    return (x5*256+x6);
}

int check_port(char *message)
{
    char *header;
    char *payload;
    char *x[6];

    header = strtok(message, "(");
    payload = strtok(NULL, ")");

    x[0] = strtok(payload,",");
    for (int i = 1; i < 4; i++)
    {
        x[i]=strtok(NULL," ,");
    }
    x[4]=strtok(NULL," ,");
    x[5]=strtok(NULL," ,");

    return (port_calc(x[4],x[5]));
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s address\n", argv[0]);
        exit(-1);
    }

    char *url = argv[1];

    url_parsing(url);
    
    printf("Host: %s\n", host);
    printf("Path: %s\n", url_path);
    printf("File: %s\n", filename+1);

    // Get IP of a name
    h = gethostbyname(host);

    if (h == NULL)
    {
        herror(host);
        exit(-1);
    }

    printf("Host name  : %s\n", h->h_name);
    printf("IP Address : %s\n\n", inet_ntoa(*((struct in_addr *)h->h_addr)));
    
    // Open a TCP socket
    int command_socket = open_socket(SERVER_PORT);
    if(command_socket < 0){
        return -1;
    }
    FILE* command_stream = fdopen(command_socket,"r");
    
    char response[MAX_SIZE]={0};
    
    if(read_from_socket(command_stream,command_socket,response) < 0){
        return -1;
    }

    //user
    if(flag != 1)
    {
        printf("Starting Default Logging Procedure\n");
        strcat(user_command,"anonymous\n");
        if(write_to_socket(command_socket,user_command)<0){
            return -1;
        }
    }
    else
    {
        if(write_to_socket(command_socket,user_command)<0){
            return -1;
        }
    }
    
    if(read_from_socket(command_stream,command_socket,response) < 0){
        return -1;
    }
    if (response[0] == '5' && response[1] == '3' && response[2] == '0')
    {
        printf("%s",response);
        exit(1);
    }

    //password
    if(flag != 1)
    {
        strcat(pass_command,"anonymous\n");
        if(write_to_socket(command_socket,pass_command)<0){
            return -1;
        }
    }
    else
    {
        if(write_to_socket(command_socket,pass_command)<0){
            return -1;
        }
    }
    if(read_from_socket(command_stream,command_socket,response) < 0){
        return -1;
    }

    //passive mode
    if(write_to_socket(command_socket,"pasv\n")<0){
        return -1;
    }
    int port;
    if(read_from_socket(command_stream, command_socket,response) < 0){
        return -1;
    }
    if (response[0] == '2' && response[1] == '2' && response[2] == '7')
    {
        port = check_port(response);
    }
    
    // Open a TCP socket
    int file_socket = open_socket(port);
    if(file_socket < 0){
        return -1;
    }
    
    char retr_command[MAX_SIZE] = "retr ./";
    
    strcat(retr_command,url_path);
    strcat(retr_command,enter);

    if(write_to_socket(command_socket,retr_command)<0){
        return -1;
    }
    if(read_from_socket(command_stream,command_socket,response) < 0){
        return -1;
    }
    if(response[0] == '5' && response[1] == '5' && response[2] == '0')
    {
        exit(1);
    }


    char response_file[MAX_SIZE]={0};
    
    FILE* file_stream = fdopen(file_socket,"r");
    int file_desc = open(filename+1, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if(file_desc < 0)
    {
        fprintf(stderr, "Error opening file: %s\n", filename);
        exit(1);
    }

    char *size;
    if (response[0] == '1' && response[1] == '5' && response[2] == '0')
    {
        size = strtok(response,"(");
        size = strtok(NULL,"b");
    }

    int file_size = atoi(size);
    int bytes_read = 0;
    double curr_size = 0.0;

    //progress bar
    double percentage = curr_size/file_size;
    int val = (int) (percentage * 100);
    int lpad = (int) (percentage * 60);
    int rpad = 60 - lpad;
    while ((bytes_read = read(file_socket, response_file, MAX_SIZE)))
    {
        if (bytes_read < 0)
        {
            perror("read()\n");
            return -1;
        }

        curr_size+=bytes_read;

        if (write(file_desc, response_file, bytes_read) < 0)
        {
            perror("write()\n");
            return -1;
        }
        
        //progress bar
        percentage = curr_size/file_size;
        val  = (int) (percentage * 100);
        lpad = (int) (percentage * 60);
        rpad = 60 - lpad;
        printf("\r%3d%% [%.*s%*s]", val, lpad, "OOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOO", rpad, "");
    }
    fflush(stdout);
    printf("\n");

    // Close file
    close(file_desc);
    
    if(read_from_socket(command_stream, command_socket,response) < 0){
        return -1;
    }

    //close sockets and file descriptors
    if(fclose(file_stream)<0)
    {
        perror("close()");
        exit(-1);
    }
    if(fclose(command_stream)<0)
    {
        perror("close()");
        exit(-1);
    }

    return 0;
}
