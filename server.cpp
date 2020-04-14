/* The port number is passed as an argument */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "mensaje.pb.h"
using namespace chat;

void error(const char *msg)
{
    perror(msg);
    exit(1);
}

int main(int argc, char *argv[])
{
    GOOGLE_PROTOBUF_VERIFY_VERSION;
    int sockfd, newsockfd, portno;
    socklen_t clilen;
    char buffer[256];
    struct sockaddr_in serv_addr, cli_addr;
    int n;
    if (argc < 2) {
        fprintf(stderr,"ERROR, no port provided\n");
        exit(1);
    }
    // create a socket
    // socket(int domain, int type, int protocol)
    sockfd =  socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
    error("ERROR opening socket");

    // clear address structure
    bzero((char *) &serv_addr, sizeof(serv_addr));

    portno = atoi(argv[1]);

    /* setup the host_addr structure for use in bind call */
    // server byte order
    serv_addr.sin_family = AF_INET;  

    // automatically be filled with current host's IP address
    serv_addr.sin_addr.s_addr = INADDR_ANY;  

    // convert short integer value for port must be converted into network byte order
    serv_addr.sin_port = htons(portno);

    // bind(int fd, struct sockaddr *local_addr, socklen_t addr_length)
    // bind() passes file descriptor, the address structure, 
    // and the length of the address structure
    // This bind() call will bind  the socket to the current IP address on port, portno
    if (bind(sockfd, (struct sockaddr *) &serv_addr,
            sizeof(serv_addr)) < 0) 
            error("ERROR on binding");

    // This listen() call tells the socket to listen to the incoming connections.
    // The listen() function places all incoming connection into a backlog queue
    // until accept() call accepts the connection.
    // Here, we set the maximum size for the backlog queue to 5.
    std::cout << "Listening..." << std::endl;
    listen(sockfd,5);

    // The accept() call actually accepts an incoming connection
    clilen = sizeof(cli_addr);

    // This accept() function will write the connecting client's address info 
    // into the the address structure and the size of that structure is clilen.
    // The accept() returns a new socket file descriptor for the accepted connection.
    // So, the original socket file descriptor can continue to be used 
    // for accepting new connections while the new socker file descriptor is used for
    // communicating with the connected client.
    newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
    if (newsockfd < 0) 
        error("ERROR on accept");

    printf("server: got connection from %s port %d\n",
        inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));

    /*
                THREE WAY HANDSHAKE
    */
    bzero(buffer,256);

    n = read(newsockfd,buffer,255);
    if (n < 0) error("ERROR reading from socket");
    
    // crear un mensaje de server para el handshake 

    ClientMessage clientMessage;
    ClientMessage clientAcknowledge;
    ServerMessage serverMessage;

        // recepcion y parse de mensaje del cliente
    clientMessage.ParseFromString(buffer);
    std::cout << "Received information: " << std::endl;
    std::cout << "Option: " << clientMessage.option() << std::endl;
    std::cout << "Username: " << clientMessage.synchronize().username() << std::endl;
    std::cout << "ip: " << clientMessage.synchronize().ip() << std::endl;

        // envio de response 
    //response build 
    MyInfoResponse * serverResponse(new MyInfoResponse); 
    serverResponse->set_userid(1);

    serverMessage.set_option(4);
    serverMessage.set_allocated_myinforesponse(serverResponse);

    std::string binary;
    serverMessage.SerializeToString(&binary);
    
    // envio de mensaje de cliente a server 
    char cstr[binary.size() + 1];
    strcpy(cstr, binary.c_str());

    // send to socket
    send(newsockfd, cstr, strlen(cstr), 0 );

    // listen for acknowledge 
    bzero(buffer,256);

    n = read(newsockfd,buffer,255);
    if (n < 0) error("ERROR reading from socket");

    clientAcknowledge.ParseFromString(buffer);
    std::cout << "Client acknowledge: " << std::endl;
    std::cout << "Option: " << clientAcknowledge.option() << std::endl;
    std::cout << "User ID: " << clientAcknowledge.acknowledge().userid() << std::endl;

    close(newsockfd);
    close(sockfd);
    return 0; 
}