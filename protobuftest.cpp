
#include <string>
#include <iostream>
#include <sys/socket.h>
#include <unistd.h>
#include "mensaje.pb.h"
using namespace std;
using namespace chat;

#define SOCKET 9999 // send socket
// #define RECEIVE_SOCKET 9999 // receive socket

int main()
{
    GOOGLE_PROTOBUF_VERIFY_VERSION;
    /*
            THREE WAY HANDSHAKE
    */

    //          -------     CLIENTE
    // Se crea instacia tipo MyInfoSynchronize y se setean los valores deseables
    MyInfoSynchronize * clientInfo(new MyInfoSynchronize);
    clientInfo->set_username("username123");
    clientInfo->set_ip("127.0.0.1");

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

    // send to socker
    send(SOCKET, cstr, strlen(cstr), 0 );


    //          -------     SERVER
    // recepcion de server de mensaje
    char *messagebuf = (char*)malloc(sizeof(char) * 30);
    read(RECEIVE_SOCKET, messagebuf, 8192);

    
    // Se deserealiza el string a un objeto Mensaje
    ClientMessage receivedMessage;
    // char *test = static_cast<char*>(messagebuf);
    receivedMessage.ParseFromString(binary); //no debe ser binary, debe ser messagebuf
    // receivedMessage.ParseFromString(message);


    // Print de mensaje recibido 
    cout << "Option: " << receivedMessage.option() << endl;
    cout << "Username: " << receivedMessage.synchronize().username() << endl;
    cout << "ip: " << receivedMessage.synchronize().ip() << endl;
    // envio de respuesta de server a cliente
    
    //response build 
    MyInfoResponse serverResponse; 
    serverResponse.set_userid(1);
        // en que momento se manda????

    
    //          -------     CLIENTE
    MyInfoAcknowledge infoAck;
    infoAck.set_userid(1);


    //library shutdown
    google::protobuf::ShutdownProtobufLibrary();
    return 1;
}