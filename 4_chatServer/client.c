// gcc client.c -o client.out && ./client.out localhost 5000

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <pthread.h>
#include <arpa/inet.h>

int sockFd, portno, n, msgId = 0;
char username[256];
#define MAX_MSG_LEN 256
#define debug 1

struct message {
    int id;
    char payload[256];
    char sender[256];
    int isOutgoing;
    int sentSuccess;
};

/**
 * Implement circular buffer later
 */
struct message messages[1000];
int msgStart = 0, msgEnd = 0;

void error(const char *msg)
{
    perror(msg);
    exit(0);
}

void getMessage(char buffer[], int sockFd, int peek)
{
    bzero(buffer, MAX_MSG_LEN);
    int n = recv(sockFd, buffer, MAX_MSG_LEN - 1, (peek ? MSG_PEEK : 0));
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
        printf("Sending message: %s\n", msg);
    int ret = send(sockFd, msg, strlen(msg), 0);

    if (ret < 0)
    {
        error("ERROR writing to socket");
    }
    char buffer[MAX_MSG_LEN];
    getMessage(buffer, sockFd, 0);

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

void addMessageToStore(char msg[], int isOutgoing, char sender[])
{
    messages[msgId].isOutgoing = isOutgoing;
    strcpy(messages[msgId].payload, msg);
    strcpy(messages[msgId].sender, sender);
    messages[msgId].sentSuccess = 0;
    messages[msgId].id = msgId;
    printf("Id: %d", msgId);
    msgId++;
}

void printMessages()
{
    system("clear");
    int i;
    for (i = 0; i < msgId; i++)
    {
        if (messages[i].isOutgoing)
        {
            printf("[%s]: %s\n", username, messages[i].payload);
        } else {
            printf("%s: %s\n", messages[i].sender, messages[i].payload);
        }

    }
}

void sendChatMessage(char msg[200])
{
    /**
     * Format: chat:[id]:[username]:message 
     */
    char payload[MAX_MSG_LEN], ackMsg[MAX_MSG_LEN];
    int ret = snprintf(payload, MAX_MSG_LEN, "chat:[%d]:[%s]:%s", msgId, username, msg);
    if( ret < 0)
        error("Error in snprintf");
    char expectdAck[MAX_MSG_LEN];
    ret = snprintf(expectdAck, MAX_MSG_LEN, "chat:[%d]", msgId);
    if( ret < 0)
        error("Error in snprintf");
    addMessageToStore(payload, 1, username);

    sendMessage(payload, ackMsg);
    if (strcmp(ackMsg, expectdAck) != 0)
    {
        printf("Chat message failed: Got: %s. Expected: %s\n", ackMsg, expectdAck);
        exit(0);
    }
}

void *getIncomingMessages(void *args)
{
    while (1)
    {
        char buffer[MAX_MSG_LEN];
        bzero(buffer, MAX_MSG_LEN);
        getMessage(buffer, sockFd, 1);

        // printf("Peeked message: %s", buffer);
        if(strncmp(buffer, "ack:", 4) == 0)
        {
            // printf("Got ack: %s", buffer);
            continue;
        } else {
            getMessage(buffer, sockFd, 0);
            char sender[50], message[MAX_MSG_LEN];
            if(strncmp(buffer, "chat:", 5) == 0)
            {
                sscanf(buffer, "chat:[%s]:%s", sender, message);
            } else {
                strcpy(sender, "server");
                strcpy(message, buffer);
            }
            addMessageToStore(message, 0, sender);
            printMessages();
        }

        // close(sockFd);
        // removeClient(sockFd);
        // return NULL;
    }
}

int isValidIpAddress(char *ipAddress)
{
    struct sockaddr_in sa;
    int result = inet_pton(AF_INET, ipAddress, &(sa.sin_addr));
    return result != 0;
}

int main(int argc, char *argv[])
{
    setbuf(stdout, NULL);
    struct sockaddr_in serv_addr;
    struct hostent *server;

    char buffer[256];
    if (argc < 3)
    {
        fprintf(stderr, "usage %s hostname/ip port\n", argv[0]);
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
    if (sockFd < 0)
        error("ERROR opening socket");
    
    bzero((char *)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(portno);
    if (isValidIpAddress(argv[1]))
    {
        serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
    } else {
        server = gethostbyname(argv[1]);
        if (server == NULL)
        {
            error("ERROR, no such host\n");
        }
        serv_addr.sin_addr.s_addr = *((unsigned long *)server->h_addr_list[0]);
    }
    printf("Server address: %s\n", inet_ntoa(serv_addr.sin_addr));
    if(connect(sockFd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        error("ERROR connecting");
    }

    if(read(sockFd, buffer, 255) < 0)
    {
        perror("ERROR reading from socket");
        close(sockFd);
    }

    if (strcmp(buffer, "success") != 0)
    {
        printf("Connection failed: %s\n", buffer);
        exit(0);
    }

    initConnection(username);
    pthread_t *thread = (pthread_t *)malloc(sizeof(pthread_t));
    pthread_create(thread, NULL, getIncomingMessages, NULL);
    while (1)
    {
        printf("Enter your message: ");
        bzero(buffer, 256);
        fgets(buffer, 255, stdin);
        sendChatMessage(buffer);
    }

    // strcpy(payload, "[");
    // strcat(payload, username);
    // strcat(payload, "]: ");
    // strcat(payload, buffer);
    // sendMessage(payload);
    // close(sockfd);
    return 0;
}

/**
 * Run two threads, one for sending messages and one for receiving messages - ok
 * TODO: prettify the output
*/
