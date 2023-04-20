// gcc server.c -pthread -o server && ./server

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/time.h>
#include <pthread.h>
#include <ifaddrs.h>

#define MAX_CLIENTS 5
#define PORT 5000
#define MAX_MSG_LEN 256
#define debug 0

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

    /**
     *  Set linger option to 0 so that close() will return immediately and
     *  not wait for the client to close the connection
     *  Also, server port will not wait in TIME_WAIT state,
     *  thus letting the server bind to the same port again immediately
     */
    struct linger so_linger;
    so_linger.l_onoff = 1;
    so_linger.l_linger = 0;
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (cliSockFds[i] != -1)
        {
            setsockopt(cliSockFds[i], SOL_SOCKET, SO_LINGER, &so_linger, sizeof(so_linger));
            close(cliSockFds[i]);
        }
    }
    setsockopt(masterSockFd, SOL_SOCKET, SO_LINGER, &so_linger, sizeof(so_linger));
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
    if (debug)
        printf("Sending message to all clients except %d\n", exceptIndex);
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
    if (debug)
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

void printServerIpAddresses()
{
    // credits for interface ip adress: 
    // https://dev.to/fmtweisszwerg/cc-how-to-get-all-interface-addresses-on-the-local-device-3pki
    printf("\nServer IP addresses:\n");
    struct ifaddrs *ptr_ifaddrs = NULL;

    int result = getifaddrs(&ptr_ifaddrs);
    if (result != 0)
    {
        error("getifaddrs()` failed: ");
    }
    
    for (
        struct ifaddrs *ptr_entry = ptr_ifaddrs;
        ptr_entry != NULL;
        ptr_entry = ptr_entry->ifa_next)
    {
        char ipaddress_human_readable_form[256];
        char netmask_human_readable_form[256];

        char interface_name[256];
        strcpy(interface_name, ptr_entry->ifa_name) ;
        sa_family_t address_family = ptr_entry->ifa_addr->sa_family;
        if (address_family == AF_INET)
        {
            // IPv4

            // Be aware that the `ifa_addr`, `ifa_netmask` and `ifa_data` fields might contain nullptr.
            // Dereferencing nullptr causes "Undefined behavior" problems.
            // So it is need to check these fields before dereferencing.
            if (ptr_entry->ifa_addr != NULL)
            {
                char buffer[INET_ADDRSTRLEN] = {
                    0,
                };
                inet_ntop(
                    address_family,
                    &((struct sockaddr_in *)(ptr_entry->ifa_addr))->sin_addr,
                    buffer,
                    INET_ADDRSTRLEN);

                strcpy(ipaddress_human_readable_form, buffer);
            }
            printf("\n%s: IP address = %s", interface_name, ipaddress_human_readable_form);
        }
        else if (address_family == AF_INET6)
        {
            //Temporarily disabling printing of IPv6 addresses
            continue;
            // IPv6
            uint32_t scope_id = 0;
            if (ptr_entry->ifa_addr != NULL)
            {
                char buffer[INET6_ADDRSTRLEN] = {
                    0,
                };
                inet_ntop(
                    address_family,
                    &((struct sockaddr_in6 *)(ptr_entry->ifa_addr))->sin6_addr,
                    buffer,
                    INET6_ADDRSTRLEN);

                strcpy(ipaddress_human_readable_form,buffer);
                scope_id = ((struct sockaddr_in6 *)(ptr_entry->ifa_addr))->sin6_scope_id;
            }
            printf("\n%s: IP address = %s", interface_name, ipaddress_human_readable_form);
        }
        else
        {
            // AF_UNIX, AF_UNSPEC, AF_PACKET etc.
            // If ignored, delete this section.
        }
    }

    freeifaddrs(ptr_ifaddrs);
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

    printServerIpAddresses();
    printf("\n\n\nServer is listening on port %d\n", PORT);

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
        if (debug)
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