// gcc client.c -o client.out && ./client.out localhost 5000

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

int sockFd, portno, n;
char username[256];
#define MAX_MSG_LEN 256
#define debug 1

void error(const char *msg)
{
    perror(msg);
    exit(0);
}

void getMessage(char buffer[], int sockFd)
{
    bzero(buffer, MAX_MSG_LEN);
    int n = recv(sockFd, buffer, MAX_MSG_LEN - 1, 0);
    if (n < 0)
    {
        perror("ERROR reading from socket");
        close(sockFd);
    }
    if (n == 0)
    {
        printf("Server disconnected\n");
        close(sockFd);
    }
}

int sendMessage(char msg[], char *ackMsg)
{
    /**
     * Remove the newline character from the end of the message
     */
    msg[strcspn(msg, "\n")] = 0;
    if (debug)
        printf("Sending message: %s", msg);
    int ret = send(sockFd, msg, strlen(msg), 0);

    if (ret < 0)
    {
        error("ERROR writing to socket");
    }
    char buffer[MAX_MSG_LEN];
    getMessage(buffer, sockFd);
    printf("Message recieved: %s", buffer);

    if (strncmp(buffer, "ack:", 4) != 0)
    {
        printf("Didn't recieve acknowledgement: %s\n", buffer);
    }
    strcpy(ackMsg, buffer + 4);
    return ret;
}

void initConnection(char username[])
{
    char payload[MAX_MSG_LEN] = "username:", ackMsg[MAX_MSG_LEN];
    strcat(payload, username);
    sendMessage(payload, ackMsg);
    if (strcmp(ackMsg, "username:success") != 0)
    {
        printf("Connection failed: %s\n", ackMsg);
        exit(0);
    }

    printf("Connected to chat server\n");
}

void sendChatMessage(char msg[])
{
    char payload[256] = "chat:", ackMsg[MAX_MSG_LEN];
    strcat(payload, "[");
    strcat(payload, username);
    strcat(payload, "]: ");
    strcat(payload, msg);
    sendMessage(payload, ackMsg);
    if (strcmp(ackMsg, "chat:success") != 0)
    {
        printf("Chat message failed: %s\n", ackMsg);
        exit(0);
    }
}

int main(int argc, char *argv[])
{
    struct sockaddr_in serv_addr;
    struct hostent *server;

    char buffer[256];
    if (argc < 3)
    {
        fprintf(stderr, "usage %s hostname port\n", argv[0]);
        exit(0);
    }

    printf("Enter username: ");
    fgets(username, 255, stdin);
    /**
     * Remove the newline character from the end of the message
     */
    username[strcspn(username, "\n")] = 0;

    portno = atoi(argv[2]);
    sockFd = socket(AF_INET, SOCK_STREAM, 0);
    printf("sockfd: %d\n", sockFd);
    if (sockFd < 0)
        error("ERROR opening socket");
    server = gethostbyname(argv[1]);
    if (server == NULL)
    {
        fprintf(stderr, "ERROR, no such host\n");
        exit(0);
    }
    bzero((char *)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(portno);
    if (connect(sockFd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        error("ERROR connecting");

    read(sockFd, buffer, 255);
    if (strcmp(buffer, "success") != 0)
    {
        printf("Connection failed: %s\n", buffer);
        exit(0);
    }

    initConnection(username);
    printf("Please enter the message: ");
    bzero(buffer, 256);
    fflush(stdin);
    fgets(buffer, 255, stdin);

    sendChatMessage(buffer);
    // strcpy(payload, "[");
    // strcat(payload, username);
    // strcat(payload, "]: ");
    // strcat(payload, buffer);
    // sendMessage(payload);
    // close(sockfd);
    return 0;
}
