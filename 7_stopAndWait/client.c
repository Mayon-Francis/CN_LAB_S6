// gcc client.c -o client.out && ./client.out localhost 5000

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#define PORT 5000
#define MAX_MSG_LEN 256

void error(const char *msg)
{
    perror(msg);
    exit(0);
}

char* recieveMsg(int sockFd, char buffer[])
{
    // char buffer[MAX_MSG_LEN];
    bzero(buffer, MAX_MSG_LEN);
    int n = recv(sockFd, buffer, MAX_MSG_LEN - 1, 0);

    if (n < 0)
    {
        error("ERROR reading from socket");
        close(sockFd);
    }
    if (n == 0)
    {
        printf("Client disconnected\n");
        close(sockFd);
        return 0;
    }

    strcat(buffer, '\0');
    return buffer;
}

int main(int argc, char *argv[])
{
    int sockfd, portno = PORT, n;
    struct sockaddr_in serv_addr;
    struct hostent *server;

    char buffer[256];
    char host[256] = "localhost";
    if (argc < 2)
    {
        printf("Hostname not provided. Defaulting to localhost\n");
    }
    else
    {
        strcpy(host, argv[1]);
    }
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        error("ERROR opening socket");
    }
    server = gethostbyname(argv[1]);
    if (server == NULL)
    {
        fprintf(stderr, "ERROR, no such host\n");
        exit(0);
    }
    bzero((char *)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(portno);
    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        error("ERROR connecting");
    }

    int frameCount = 0;
    printf("Enter number of frames to send: ");
    scanf("%d", &frameCount);

    for (int i = 0; i < frameCount; i++)
    {
        strcpy(buffer, "FRAME " + '0' + i);
        send(sockfd, buffer, strlen(buffer), 0);
        strcpy(buffer, recieveMsg(sockfd, buffer));
        printf("Message received: %s \n", buffer);
    }
    close(sockfd);
    return 0;
}

