#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int MAXIMUM_CONNECTIONS_NUMBER = 128;
int LISTENING_SOCKET_INDEX = 128;
int USER_INTERACTION_INDEX = 129;

int printRemoteIpAddr(int fd)
{
    socklen_t addr_size;
    struct sockaddr sockAddr;
    getpeername(fd, &sockAddr, &addr_size);
    char ip[20];
    inet_ntop(AF_INET, &(((struct sockaddr_in*)&sockAddr)->sin_addr), ip, 20);
    printf("%s", ip);
    return 0;
}

int handleNewConnection(int listeningSocket, int* clients)
{
    struct sockaddr newAddress;
    socklen_t newAddressLen;
    int newConnectionFd = accept(listeningSocket, &newAddress, &newAddressLen);

    printf("New connection incoming IP: ");
    printRemoteIpAddr(newConnectionFd);
    printf("\n");
    for (int i = 0; i < MAXIMUM_CONNECTIONS_NUMBER; i++)
    {
        if (clients[i] == -1)
        {
            printf("New client accepted! IP: ");
            printRemoteIpAddr(newConnectionFd);
            printf(" Client id: #%d\n", i);
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
int handleUserInput(int* isServerRunning, int* clients)
{
    char buffer[1024];
    bzero(buffer, sizeof(buffer));
    read(STDIN_FILENO, buffer, sizeof(buffer));

    char action;
    int user;
    char text[128];

    sscanf(buffer, "%c %d %s", &action, &user, text);
    switch (action)
    {
        case 'e':
        {
            *isServerRunning = 0;
            return 0;
        }
        case 'w':
        {
            if (user >= 0 && user < MAXIMUM_CONNECTIONS_NUMBER && clients[user] != -1)
            {
                int size = write(clients[user], text, strlen(text));
                printf("%d bytes wrote to client #%d\n", size, user);
                return 0;
            }
            else
            {
                fprintf(stderr, "Wrong user number or user not connected\n");
                return 0;
            }
        }
        case 'l':
        {
            printf("Active users:\n");
            for (int i = 0; i < MAXIMUM_CONNECTIONS_NUMBER; i++)
            {
                if (clients[i] != -1)
                {
                    printf("Client #%d IP: ", i);
                    printRemoteIpAddr(clients[i]);
                    printf("\n");
                }
            }
        }
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

        poll(fdsToPoll, MAXIMUM_CONNECTIONS_NUMBER + 2, -1);
        if (fdsToPoll[LISTENING_SOCKET_INDEX].revents & POLLIN)
        {
            handleNewConnection(listeningSocket, clients);
        }

        handleDataFromClients(fdsToPoll, clients);

        if (fdsToPoll[USER_INTERACTION_INDEX].revents & POLLIN)
        {
            handleUserInput(&isServerRunning, clients);
        }
    }
    close(listeningSocket);
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
    printf("Waiting for action...\n\tw 1 text - writes \"text\" to user #1\n\tl - list users\n\te - exits "
           "program "
           "programm\n");
    int clients[MAXIMUM_CONNECTIONS_NUMBER];
    memset(&clients, -1, sizeof(int) * MAXIMUM_CONNECTIONS_NUMBER);
    handleConnections(listeningSocket, &clients);

    return 0;
}
