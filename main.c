#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int MAXIMUM_CONNECTIONS_NUMBER = 128;
int LISTENING_SOCKET_INDEX = 128;
int USER_INTERACTION_INDEX = 129;

int handleNewConnection(int listeningSocket, int* clients)
{
    struct sockaddr newAddress;
    socklen_t newAddressLen;
    int newConnectionFd = accept(listeningSocket, &newAddress, &newAddressLen);
    char ip[20];
    inet_ntop(AF_INET, &(((struct sockaddr_in*)&newAddress)->sin_addr), ip, 20);
    printf("New connection incoming IP: %s\n", ip);
    for (int i = 0; i < MAXIMUM_CONNECTIONS_NUMBER; i++)
    {
        if (clients[i] == -1)
        {
            printf("New client accepted! IP: %s Client id: #%d\n", ip, i);
            clients[i] = newConnectionFd;
            return 0;
        }
    }
    fprintf(stderr, "No room for new connection!\n");
    close(newConnectionFd);
}
int handleDataFromClients(struct pollfd* polledFds, int* clients)
{
    const int bufferSize = 1024 * 32;
    char buffer[bufferSize];
    for (int i = 0; i < MAXIMUM_CONNECTIONS_NUMBER; i++)
    {
        if (polledFds[i].revents & POLLIN)
        {
            bzero(buffer, bufferSize);
            int readSize = read(polledFds[i].fd, buffer, sizeof(buffer));
            if (readSize == 0)
            {
                printf("Read from client #%d empty. Considered as disconnected\n", i);
                close(clients[i]);
                clients[i] = -1;
            }
            else
            {
                printf("Client #%d wrote:\n%s\n", i, buffer);
            }
        }
        if (polledFds[i].revents & (POLLHUP | POLLERR))
        {
            printf("Client #%d disconnected\n", i);
            close(clients[i]);
            clients[i] = -1;
            return 0;
        }
    }
}
int handleUserInput(int* isServerRunning)
{
    char buff[1024];
    read(STDIN_FILENO, buff, sizeof(buff));
    if (strncmp(buff, "exit", 4) == 0)
    {
        *isServerRunning = 0;
    }
}
int handleConnections(int listeningSocket, int* clients)
{
    int isServerRunning = 1;
    while (isServerRunning == 1)
    {
        struct pollfd fdsToPoll[MAXIMUM_CONNECTIONS_NUMBER + 2];
        struct pollfd listeningSocketPoll;
        listeningSocketPoll.fd = listeningSocket;
        listeningSocketPoll.events = POLLIN;
        fdsToPoll[LISTENING_SOCKET_INDEX] = listeningSocketPoll;

        struct pollfd userInteractionPoll;
        userInteractionPoll.fd = STDIN_FILENO;
        userInteractionPoll.events = POLLIN;
        fdsToPoll[USER_INTERACTION_INDEX] = userInteractionPoll;

        for (int i = 0; i < MAXIMUM_CONNECTIONS_NUMBER; i++)
        {
            struct pollfd clientPoll;
            clientPoll.fd = clients[i];
            clientPoll.events = POLLIN;
            fdsToPoll[i] = clientPoll;
        }
        printf("Waiting for action...\n");
        poll(fdsToPoll, MAXIMUM_CONNECTIONS_NUMBER + 2, -1);
        if (fdsToPoll[LISTENING_SOCKET_INDEX].revents & POLLIN)
        {
            handleNewConnection(listeningSocket, clients);
        }

        handleDataFromClients(fdsToPoll, clients);

        if (fdsToPoll[USER_INTERACTION_INDEX].revents & POLLIN)
        {
            handleUserInput(&isServerRunning);
        }
    }
    return 0;
}

int main(int argc, char* argv[])
{
    setvbuf(stdout, NULL, _IONBF, 0);

    int portNumber = -1;
    for (int arg = 1; arg < argc; arg++)
    {
        if (strcmp(argv[arg], "-p") == 0)
        {
            if (arg + 1 < argc)
            {
                portNumber = atoi(argv[arg + 1]);
                arg++;
            }
        }
    }
    if (portNumber == -1)
    {
        fprintf(stderr, "No port defined. Please use -p option.\n");
        return 0;
    }

    int listeningSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (listeningSocket == -1)
    {
        fprintf(stderr, "Unable to create listening socket.\n");
        return 0;
    }
    struct sockaddr_in servaddr;
    bzero(&servaddr, sizeof(servaddr));

    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(portNumber);

    if ((bind(listeningSocket, (struct sockaddr*)&servaddr, sizeof(servaddr))) != 0)
    {
        fprintf(stderr, "Unable to bind.\n");
        return 0;
    }

    if ((listen(listeningSocket, MAXIMUM_CONNECTIONS_NUMBER)) != 0)
    {
        fprintf(stderr, "Unable to listen, check if port is free.\n");
        return 0;
    }

    printf("Server is ready to use! Listening port: %d\n", portNumber);

    int clients[MAXIMUM_CONNECTIONS_NUMBER];
    memset(&clients, -1, sizeof(int) * MAXIMUM_CONNECTIONS_NUMBER);
    handleConnections(listeningSocket, &clients);

    return 0;
}
