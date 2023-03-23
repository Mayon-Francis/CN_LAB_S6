// gcc server.c -o server && ./server

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/time.h>
#include <pthread.h>

#define PORT 5000
#define MAX_MSG_LEN 256
#define MAX_CLIENTS 1

void error(const char *msg)
{
    perror(msg);
    exit(0);
}

int main()
{

    int mainSockFd = socket(PF_INET, SOCK_STREAM, 0);
    if (mainSockFd < 0)
    {
        perror("ERROR opening socket");
        exit(1);
    }

    struct sockaddr_in servAddr;
    bzero((char *)&servAddr, sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    servAddr.sin_addr.s_addr = INADDR_ANY;
    servAddr.sin_port = htons(PORT);

    if (bind(mainSockFd, (struct sockaddr *)&servAddr, sizeof(servAddr)) < 0)
    {
        perror("ERROR on binding");
        exit(1);
    }

    if (listen(mainSockFd, MAX_CLIENTS) == -1)
    {
        perror("ERROR on listen");
        exit(1);
    }

    printf("Server is listening on port %d\n", PORT);

    while (1)
    {
        struct sockaddr_in cliAddr;
        socklen_t cliLen = sizeof(cliAddr);
        int newSockFd = accept(mainSockFd, (struct sockaddr *)&cliAddr, &cliLen);
        if (newSockFd < 0)
        {
            error("ERROR on accept");
        }
        char buffer[MAX_MSG_LEN];
        bzero(buffer, MAX_MSG_LEN);
        int n = recv(newSockFd, buffer, MAX_MSG_LEN - 1, 0);
        if (n < 0)
        {
            error("ERROR reading from socket");
            close(newSockFd);

        }
        if (n == 0)
        {
            printf("Client disconnected\n");
            close(newSockFd);
            return 0;
        }
        
        printf("Msg: %s\n", buffer);
    }
}