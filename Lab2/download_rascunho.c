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

//tratamento do argumento IP
//gethostbyname
//connect com o IP do gethostbyname
//temos que fazer login com user anonymous and pass test
//pasv temos que converter?
//no port 20 um nova conexao
//retr do ficheiro o url_path ex:pub/ubuntu-feup-legacy/2010/ubuntu-feup/changelog.txt
//enviar para o ficheiro no diretorio
//close

//diff no fim para comparar

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
    //printf("%s\n", payload);
    
    x[0] = strtok(payload,",");
    for (int i = 1; i < 4; i++)
    {
        x[i]=strtok(NULL," ,");
    }
    x[4]=strtok(NULL," ,");
    x[5]=strtok(NULL," ,");
    //printf("%s %s\n", x[4], x[5]);
    
    return (port_calc(x[4],x[5]));
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s address\n", argv[0]);
        exit(-1);
    }

    char *address_name = argv[1];

    char *trash;
    trash = strtok(address_name, "//");
    char *host;
    host = strtok(NULL, "/");
    char *url_path;
    url_path = strtok(NULL, "");
    char *filename;
    filename = strrchr(url_path, '/');
    // if (filename != NULL)
    // {
    //     printf("Last token: '%s'\n", filename+1);
    // }
    printf("%s\n", host);
    printf("%s\n", url_path);
    printf("%s\n", filename+1);

    // Get IP of a name
    struct hostent *h = gethostbyname(host);

    if (h == NULL)
    {
        herror("gethostbyname");
        exit(-1);
    }

    printf("Host name  : %s\n", h->h_name);
    printf("IP Address : %s\n", inet_ntoa(*((struct in_addr *)h->h_addr)));

    // Server address handling
    struct sockaddr_in server_addr;

    bzero((char*)&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(inet_ntoa(*((struct in_addr *)h->h_addr)));  // 32 bit Internet address network byte ordered
    server_addr.sin_port = htons(SERVER_PORT);  // Server TCP port must be network byte ordered

    // Open a TCP socket
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd < 0)
    {
        perror("socket()");
        exit(-1);
    }

    // Connect to the server
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("connect()");
        exit(-1);
    }

    char response[1024]={0};
    FILE* file = fdopen(sockfd,"r");

    while (response[3] != ' ')
    {
        memset(response, 0, 1024);
        fgets(response, 1024, file);
        printf("%s",response);
    }

    int bytesWritten;

    if((bytesWritten = write(sockfd,"user anonymous\n",strlen("user anonymous\n"))) < strlen("user anonymous\n"))
    {
        perror("Error writing command to socket\n");
        return -1;
    }

    printf("Starting Logging Procedure\n");

    while (response[0] != '3' || response[1] != '3' || response[2] != '1')
    {
        memset(response, 0, 1024);
        fgets(response, 1024, file);
        //printf("%s",response); -> need to print?
    }

    if((bytesWritten = write(sockfd,"pass test\n",strlen("pass test\n"))) < strlen("pass test\n"))
    {
        perror("Error writing command to socket\n");
        return -1;
    }

    while (response[0] != '2' || response[1] != '3' || response[2] != '0')
    {
        memset(response, 0, 1024);
        fgets(response, 1024, file);
        printf("%s",response);
    }

    if((bytesWritten = write(sockfd,"pasv\n",strlen("pasv\n"))) < strlen("pasv\n"))
    {
        perror("Error writing command to socket\n");
        return -1;
    }
    //printf("%d\n", bytesWritten);

    int port;

    //resulta pq o while so vai verificar a condiçao apos efetuar o loop
    while (response[0] != '2' || response[1] != '2' || response[2] != '7')
    {
        memset(response, 0, 1024);
        fgets(response, 1024, file);
        printf("%s",response);
        if (response[0] == '2' && response[1] == '2' && response[2] == '7')
        {
            port = check_port(response);
        }
    }

    //printf("%d\n", port);

    bzero((char*)&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(inet_ntoa(*((struct in_addr *)h->h_addr)));  // 32 bit Internet address network byte ordered
    server_addr.sin_port = htons(port);  // Server TCP port must be network byte ordered

    // Open a TCP socket
    int sockfd2 = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd2 < 0)
    {
        perror("socket()");
        exit(-1);
    }

    // Connect to the server
    if (connect(sockfd2, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("connect()");
        exit(-1);
    }

    //define com max size
    char path[1024];

    sprintf(path, "retr ./%s\n", url_path);

    printf("%s", path);

    if((bytesWritten = write(sockfd,path,strlen(path))) < strlen(path))
    {
        perror("Error writing command to socket\n");
        return -1;
    }

    //printf("%d\n",bytesWritten);

    char response2[1024]={0};
    FILE* file2 = fdopen(sockfd2,"r");

    int file_desc = open(filename+1, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if(file_desc < 0)
    {
        fprintf(stderr, "Error opening file: %s\n", filename);
        exit(1);
    }

    while (response[0] != '1' || response[1] != '5' || response[2] != '0')
    {
        memset(response, 0, 1024);
        fgets(response, 1024, file);
        printf("%s",response);
    }

    char *size;
    if (response[0] == '1' && response[1] == '5' && response[2] == '0')
    {
        size = strtok(response,"(");
        size = strtok(NULL,"b");
        //printf("%s bytes\n",size);
    }
    

    int file_size = atoi(size);
    int bytes_read = 0;
    double curr_size = 0.0;
    

    //progress bar
    double percentage = curr_size/file_size;
    int val = (int) (percentage * 100);
    int lpad = (int) (percentage * 60);
    int rpad = 60 - lpad;
    while ((bytes_read = read(sockfd2, response2, 1024)))
    {
        if (bytes_read < 0)
        {
            perror("read()\n");
            return -1;
        }
        
        curr_size+=bytes_read;

        if (write(file_desc, response2, bytes_read) < 0)
        {
            perror("write()\n");
            return -1;
        }
        //progress bar
        percentage = curr_size/file_size;
        val = (int) (percentage * 100);
        lpad = (int) (percentage * 60);
        rpad = 60 - lpad;
        printf("\r%3d%% [%.*s%*s]", val, lpad, "OOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOO", rpad, "");
    }
    fflush(stdout);
    printf("\n");
    // Close file
    close(file_desc);

    while (response[0] != '2' || response[1] != '2' || response[2] != '6')
    {
        memset(response, 0, 1024);
        fgets(response, 1024, file);
        printf("%s",response);
    }

    //print que a ligaçao foi fechada ver notas

    // Close socket2
    if (close(sockfd2) < 0)
    {
        perror("close()");
        exit(-1);
    }

    // Close socket
    if (close(sockfd) < 0)
    {
        perror("close()");
        exit(-1);
    }
    system("diff -s changelog.txt changelog2.txt");
    return 0;
}