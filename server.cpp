#include <iostream>
#include <fstream>
#include <string>
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <arpa/inet.h>
#include <unordered_map>
#include "mensaje.pb.h"
using namespace chat;

#define BACKLOG 5
#define MAX_BUFFER 8192
#define CLI_OPTION_SYNC 1
#define CLI_OPTION_USERS 2
#define CLI_OPTION_STAT 3
#define CLI_OPTION_BROADCAST 4
#define CLI_OPTION_DM 5

struct ChatClient
{
    int socketFd;
    std::string username;
    char ipAddr[INET_ADDRSTRLEN];
    std::string status;
};

std::unordered_map<std::string, ChatClient *> clients;



void error(const char *msg)
{
    perror(msg);
    exit(1);
}



void ErrorToClient(int socketFd, std::string errorMsg)
{
    std::string msgSerialized;
    ErrorResponse *errorMessage(new ErrorResponse);
    errorMessage->set_errormessage(errorMsg);
    chat::ServerMessage serverMessage;
    serverMessage.set_option(3);
    serverMessage.set_allocated_error(errorMessage);
    serverMessage.SerializeToString(&msgSerialized);
    char buffer[msgSerialized.size() + 1];
    strcpy(buffer, msgSerialized.c_str());
    send(socketFd, buffer, sizeof buffer, 0);
}

void *worker_thread(void *params)
{
    struct ChatClient thisClient;
    int socketFd = *(int *)params;
    char buffer[MAX_BUFFER];
    std::string msgSerialized;
    ClientMessage clientMessage;
    ClientMessage clientAcknowledge;
    ServerMessage serverMessage;


    std::cout << "Thread for client with socket: " << socketFd << std::endl;

    while (1)
    {

        recv(socketFd, buffer, MAX_BUFFER, 0);
        
        // recepcion y parse de mensaje del cliente
        clientMessage.ParseFromString(buffer);

        // Un if para cada opcion del cliente
        if (clientMessage.option() == CLI_OPTION_SYNC)
        {

            printf("Siempre esta entrando aqui \n");
            if (!clientMessage.has_synchronize())
            {
                ErrorToClient(socketFd, "No Synchronize information");
                break;
            }

            /*
                THREE WAY HANDSHAKE
            */            
            std::cout << "Received information: " << std::endl;
            std::cout << "Option: " << clientMessage.option() << std::endl;
            std::cout << "Username: " << clientMessage.synchronize().username() << std::endl;
            std::cout << "ip: " << clientMessage.synchronize().ip() << std::endl;


                // envio de response 
            //response build 
            MyInfoResponse * serverResponse(new MyInfoResponse); 
            serverResponse->set_userid(1);

            serverMessage.Clear();
            serverMessage.set_option(4);
            serverMessage.set_allocated_myinforesponse(serverResponse);

            serverMessage.SerializeToString(&msgSerialized);

            // enviar de mensaje de cliente a server
            strcpy(buffer, msgSerialized.c_str());
            send(socketFd, buffer, msgSerialized.size() + 1, 0);
            std::cout << "MyInfoResponse send to socket: "<< socketFd << std::endl;

            recv(socketFd, buffer, MAX_BUFFER, 0);

            clientAcknowledge.ParseFromString(buffer);
            std::cout << "Client acknowledge: " << std::endl;
            std::cout << "Option: " << clientAcknowledge.option() << std::endl;
            std::cout << "User ID: " << clientAcknowledge.acknowledge().userid() << std::endl;


            strcpy(thisClient.ipAddr, clientMessage.synchronize().ip().c_str());
            thisClient.username = clientMessage.synchronize().username();
            thisClient.socketFd = socketFd;
            clients[thisClient.username] = &thisClient;
            std::cout << "User connected"<< thisClient.username<< std::endl;
        } else if (clientMessage.option() == CLI_OPTION_BROADCAST){

            printf("ENTRO \n");
            

        }


        std::cout << "--- Users:  " << clients.size() << std::endl;
        clientMessage.Clear();


        
    }

    clients.erase(thisClient.username);
    close(socketFd);
  
    std::cout << "Closing socket"<< std::endl;
    pthread_exit(0);
}

int main(int argc, char *argv[])
{
    GOOGLE_PROTOBUF_VERIFY_VERSION;
    int sockfd, newsockfd;
    socklen_t clilen;
    sockaddr_in serv_addr, cli_addr;
    long port = strtol(argv[1], NULL, 10);
    char cli_addr_addr[INET_ADDRSTRLEN];

    if (argc < 2)
    {
        fprintf(stderr, "ERROR, no port provided\n");
        exit(1);
    }
    // create a socket
    // socket(int domain, int type, int protocol)
    sockfd =  socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
    error("ERROR opening socket");
    
    // clear address structure
    bzero((char *) &serv_addr, sizeof(serv_addr));

    /* setup the host_addr structure for use in bind call */
    // server byte order
    serv_addr.sin_family = AF_INET;  

    // automatically be filled with current host's IP address
    serv_addr.sin_addr.s_addr = INADDR_ANY;  

    // convert short integer value for port must be converted into network byte order
    serv_addr.sin_port = htons(port);

    memset(serv_addr.sin_zero, 0, sizeof serv_addr.sin_zero);

    // bind(int fd, struct sockaddr *local_addr, socklen_t addr_length)
    // bind() passes file descriptor, the address structure, 
    // and the length of the address structure
    // This bind() call will bind  the socket to the current IP address on port, portno
    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
            close(sockfd);
            error("ERROR on binding\n");
    }

    // This listen() call tells the socket to listen to the incoming connections.
    // The listen() function places all incoming connection into a backlog queue
    // until accept() call accepts the connection.
    // Here, we set the maximum size for the backlog queue to 5.
    if (listen(sockfd, BACKLOG) == -1)
    {
        close(sockfd);
        error("ERROR on listening.\n");
    }

    printf("Listening...  %ld\n", port);

    while (1)
    {
        // The accept() call actually accepts an incoming connection
        clilen = sizeof cli_addr;

        // This accept() function will write the connecting client's address info 
        // into the the address structure and the size of that structure is clilen.
        // The accept() returns a new socket file descriptor for the accepted connection.
        // So, the original socket file descriptor can continue to be used 
        // for accepting new connections while the new socker file descriptor is used for
        // communicating with the connected client.
        newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
        if (newsockfd < 0)
            error("ERROR en accept()");
         

        pthread_t thread_cli;
        pthread_create(&thread_cli, NULL, worker_thread, (void *)&newsockfd);
    }

    return 0;
}