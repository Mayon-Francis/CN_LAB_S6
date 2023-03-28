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

int cliSockFds[MAX_CLIENTS], cliCount = -1, masterSockFd;
pthread_mutex_t lock;

void interruptHandler(int sig)
{
    printf("Caught signal %d \n", sig);
    printf("Closing all connections \n");
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (cliSockFds[i] != -1)
        {
            close(cliSockFds[i]);
        }
    }
    close(masterSockFd);
    exit(0);
}

int sendMessageToSockFd(char msg[], int sockFd)
{
    int ret = send(sockFd, msg, strlen(msg), 0);

    if (ret < 0)
    {
        perror("ERROR writing to socket");
    }
    return ret;
}

int sendMessageToAllClients(char msg[], int exceptIndex)
{

    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (cliSockFds[i] != -1 && i != exceptIndex)
        {
            sendMessageToSockFd(msg, cliSockFds[i]);
        }
    }
}

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

void *monitorClient(void *args)
{
    while (1)
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

        /**
         * Remove the newline character from the end of the message
         */
        buffer[strcspn(buffer, "\n")] = 0;

        if (strcmp(buffer, "exit") == 0)
        {
            printf("Client disconnected through exit command\n");
            close(sockFd);
            removeClient(sockFd);
            return NULL;
        }

        if (strncmp(buffer, "username:", 9) == 0)
        {
            printf("%s joined the chat!\n", buffer + 9);
            sendMessageToAllClients(buffer + 9, *index);
            sendMessageToSockFd("ack:username:success", sockFd);
        }

        if (strncmp(buffer, "chat:", 5) == 0)
        {
            printf("%s\n", buffer + 5);
            sendMessageToSockFd("ack:chat:success", sockFd);
        }

        // close(sockFd);
        // removeClient(sockFd);
        // return NULL;
    }
}

int main()
{
    signal(SIGINT, interruptHandler);
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        cliSockFds[i] = -1;
    }

    masterSockFd = socket(PF_INET, SOCK_STREAM, 0);
    if (masterSockFd < 0)
    {
        perror("ERROR opening socket");
        exit(1);
    }

    struct sockaddr_in servAddr;
    bzero((char *)&servAddr, sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    servAddr.sin_addr.s_addr = INADDR_ANY;
    servAddr.sin_port = htons(PORT);

    if (bind(masterSockFd, (struct sockaddr *)&servAddr, sizeof(servAddr)) < 0)
    {
        perror("ERROR on binding");
        exit(1);
    }

    if (listen(masterSockFd, MAX_CLIENTS) == -1)
    {
        perror("ERROR on listen");
        exit(1);
    }

    printf("Server is listening on port %d\n", PORT);

    while (1)
    {
        struct sockaddr_in cliAddr;
        socklen_t cliLen = sizeof(cliAddr);
        int newSockFd = accept(masterSockFd, (struct sockaddr *)&cliAddr, &cliLen);
        if (newSockFd < 0)
        {
            perror("ERROR on accept");
            exit(1);
        }
        int index = addClient(newSockFd);
        if (index == -1)
        {
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