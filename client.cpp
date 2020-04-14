#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include "mensaje.pb.h"
using namespace std;
using namespace chat;

void error(const char *msg)
{
    perror(msg);
    exit(0);
}

int main(int argc, char *argv[])
{
    GOOGLE_PROTOBUF_VERIFY_VERSION;
    int sockfd, portno, n;
    struct sockaddr_in serv_addr;
    struct hostent *server;

    char buffer[256];
    
    if (argc != 4) {
       fprintf(stderr, "./client [username] [host] [port]\n");
       exit(1);
    }
    portno = atoi(argv[3]);
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");
    server = gethostbyname(argv[2]);
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);
    }
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, 
         (char *)&serv_addr.sin_addr.s_addr,
         server->h_length);
    serv_addr.sin_port = htons(portno);
    if (connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) 
        error("ERROR connecting");

    cout << "connect" << endl;
    /*
                THREE WAY HANDSHAKE
    */

    MyInfoSynchronize * clientInfo(new MyInfoSynchronize);
    clientInfo->set_username(argv[1]);
    clientInfo->set_ip(argv[2]);

    // Se crea instancia de Mensaje, se setea los valores deseados
    ClientMessage clientMessage;
    clientMessage.set_option(1);
    clientMessage.set_allocated_synchronize(clientInfo);

    // Se serializa el message a string
    string binary;
    clientMessage.SerializeToString(&binary);
    
    // envio de mensaje de cliente a server 
    char cstr[binary.size() + 1];
    strcpy(cstr, binary.c_str());

    // send to socket
    send(sockfd, cstr, strlen(cstr), 0 );

        // listen for server response
    bzero(buffer,256);
    n = read(sockfd, buffer, 255);
    if (n < 0) 
         error("ERROR reading from socket");

        //read server response
    ServerMessage serverResponseMsg;
    serverResponseMsg.ParseFromString(buffer);
    cout << "Server response: " << endl;
    std::cout << "Option: " << serverResponseMsg.option() << std::endl;
    std::cout << "User Id: " << serverResponseMsg.myinforesponse().userid() << std::endl;
    
    // client response (acknowledge)
    MyInfoAcknowledge * infoAck(new MyInfoAcknowledge);
    infoAck->set_userid(1);
    ClientMessage clientAcknowledge; //no se como hacerle "clear" al clientmessage entonces creare otro :( 
    clientAcknowledge.set_option(1);
    clientAcknowledge.set_userid(2); //hay que cambiarlo para que sea dinamico
    clientAcknowledge.set_allocated_acknowledge(infoAck);
    // Se serializa el message a string
    string binarya;
    clientAcknowledge.SerializeToString(&binarya);
    
    // envio de mensaje de cliente a server 
    char cstr2[binarya.size() + 1];
    strcpy(cstr2, binarya.c_str());

    send(sockfd, cstr2, strlen(cstr2), 0);

    close(sockfd);
    return 0;
}