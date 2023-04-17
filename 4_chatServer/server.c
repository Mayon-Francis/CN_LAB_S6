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
char usernames[MAX_CLIENTS][50];
pthread_mutex_t lock;

void error(const char *msg)
{
    perror(msg);
    exit(0);
}

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
    printf("Sending message to all clients except %d\n", exceptIndex);
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (cliSockFds[i] != -1 && i != exceptIndex)
        {
            sendMessageToSockFd(msg, cliSockFds[i]);
        } else if (i == exceptIndex)
        {
            printf("Not sending to %d with sockFd %d\n", i, cliSockFds[i]);
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
void disconnectClient(int index)
{
    int sockFd = cliSockFds[index];
    close(sockFd);
    removeClient(sockFd);
    char broadcastMsg[MAX_MSG_LEN];
    int ret = sprintf(broadcastMsg, "%s has disconnected", usernames[index]);
    if (ret < 0)
        error("ERROR in sprintf");
    
    sendMessageToAllClients(broadcastMsg, index);
}

void *monitorClient(void *args)
{
    int *indexPtr = args;
    int index = *indexPtr;
    int sockFd = cliSockFds[index];
    printf("Monitoring client: %d with sockfd %d\n", index, sockFd);
    while (1)
    {
        char buffer[MAX_MSG_LEN];
        bzero(buffer, MAX_MSG_LEN);
        int n = recv(sockFd, buffer, MAX_MSG_LEN - 1, 0);
        if (n < 0)
        {
            perror("ERROR reading from socket");
            disconnectClient(index);
            return NULL;
        }
        if (n == 0)
        {
            printf("Client disconnected\n");
            disconnectClient(index);
            return NULL;
        }

        /**
         * Remove the newline character from the end of the message
         */
        buffer[strcspn(buffer, "\n")] = 0;

        if (strcmp(buffer, "exit") == 0)
        {
            printf("Client disconnected through exit command\n");
            disconnectClient(index);
            return NULL;
        }

        if (strncmp(buffer, "username:", 9) == 0)
        {
            char *username = buffer + 9;
            printf("%s joined the chat!\n", username);
            strcpy(usernames[index], username);
            char broadcast[MAX_MSG_LEN];
            int ret = snprintf(broadcast, MAX_MSG_LEN, "%s joined the chat!", username);
            if (ret < 0)
                error("Error in snprintf");
            sendMessageToAllClients(broadcast, index);
            sendMessageToSockFd("ack:username:success", sockFd);
        }

        if (strncmp(buffer, "chat:", 5) == 0)
        {
            /**
             *  Incoming format: chat:[id]:[username]:message
             */
            char *msgId = strtok(buffer + 5, ":");

            /**
             *  Broadcast format: chat:[username]:message
             */
            char *username = strtok(NULL, ":");
            char *msg = strtok(NULL, ":");
            char broadcast[MAX_MSG_LEN];
            int ret = snprintf(broadcast, MAX_MSG_LEN, "chat:%s:%s", username, msg);
            if (ret < 0)
                error("Error in snprintf");

            printf("Broadcast: %s\n", broadcast);
            sendMessageToAllClients(broadcast, index);

            /**
             *  Ack format: ack:chat:[id]
             */
            char payload[MAX_MSG_LEN] = "ack:chat:";
            strcat(payload, msgId);
            sendMessageToSockFd(payload, sockFd);

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
        printf("\nClient added at index %d\n", index);
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