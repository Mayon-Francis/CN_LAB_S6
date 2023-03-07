// gcc server.c -pthread -o server && ./server

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

#define MAX_CLIENTS 5
#define PORT 5000
#define MAX_MSG_LEN 256

int cliSockFds[MAX_CLIENTS], cliCount = -1;

/**
 * Add a client to the array of client socket fds
 * Returns the index of the client socket fd in the array
*/
int addClient(int sockFd)
{
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (cliSockFds[i] == -1)
        {
            cliSockFds[i] = sockFd;
            return i;
        }
    }
    printf("ERROR: Too many clients\n");
    return -1;
}

void removeClient(int sockFd)
{
    for (int i = 0; i <= MAX_CLIENTS; i++)
    {
        if (cliSockFds[i] == sockFd)
        {
            cliSockFds[i] = -1;
            return;
        }
    }
    printf("ERROR: Client not found\n");
    return;
}

void* monitorClient(void* args)
{
    int *index = args;
    int sockFd = cliSockFds[*index];
    char buffer[MAX_MSG_LEN];
    bzero(buffer, MAX_MSG_LEN);
    int n = recv(sockFd, buffer, MAX_MSG_LEN - 1, 0);
    if (n < 0)
    {
        perror("ERROR reading from socket");
        close(sockFd);
        removeClient(sockFd);
        return NULL;
    }
    if (n == 0)
    {
        printf("Client disconnected\n");
        close(sockFd);
        removeClient(sockFd);
        return NULL;
    }
    printf("Here is the message: %s", buffer);
    n = send(sockFd, "I got your message", 18, 0);
    if (n < 0)
    {
        perror("ERROR writing to socket");
        // exit(1);
    } else {
        printf("Message sent\n");
    }
    close(sockFd);
    removeClient(sockFd);
    return NULL;
}

int main()
{
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        cliSockFds[i] = -1;
    }

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
            perror("ERROR on accept");
            exit(1);
        }
        int index = addClient(newSockFd);
        if(index == -1) {
            send(newSockFd, "reject: too many clients", 24, 0);
            close(newSockFd);
            printf("Client rejected\n");
            continue;
        }
        send(newSockFd, "success", 7, 0);
        pthread_t *thread = (pthread_t *)malloc(sizeof(pthread_t));
        pthread_create(thread, NULL, monitorClient, (void *)&index);

    }

}