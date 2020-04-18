#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <pthread.h>
#include "mensaje.pb.h"
using namespace std;
using namespace chat;

// opciones de mensaje para el server
enum ServerOpt {
    BROADCAST_S = 1,
    MESSAGE = 2,
    ERROR = 3,
    RESPONSE = 4,
    C_USERS_RESPONSE = 5,
    CHANGE_STATUS = 6,
    BROADCAST_RESPONSE = 7,
    DM_RESPONSE = 8
};

void error(const char *msg)
{
    perror(msg);
    exit(0);
}

void *listen_thread(void *params){
    int socketFd = *(int *)params;
    char buffer[1024];
    string msgSerialized;
    ClientMessage clientMessage;
    ClientMessage clientAcknowledge;
    ServerMessage serverMessage;

    printf("Entro al thread\n");

    while (1)
    {

        recv(socketFd, buffer, 1024, 0);
        // recepcion y parse de mensaje del server
        serverMessage.ParseFromString(buffer);

        if (serverMessage.option() == ServerOpt::BROADCAST_RESPONSE)
        {
            cout << "UserId: " << serverMessage.broadcast().userid() << endl;
            cout << "Mensaje: " << serverMessage.broadcast().message() << endl;

        } 

        serverMessage.Clear();
    }
    close(socketFd);
    pthread_exit(0);
}

void broadCast(char buffer[], int sockfd){
    printf("ENTRO");
    string message = "Hola mundo";
    string binary;
    ClientMessage clientMessage;
    ServerMessage serverResponseMsg;

    BroadcastRequest *brdRequest = new BroadcastRequest();

    clientMessage.set_option(4);
    brdRequest->set_message(message);
    clientMessage.set_allocated_broadcast(brdRequest);
    clientMessage.SerializeToString(&binary);

    // envio de mensaje de cliente a server 
    char cstr[binary.size() + 1];
    strcpy(cstr, binary.c_str());

    // send to socket
    send(sockfd, cstr, strlen(cstr), 0 );

    printf("en teoria llega al send");

    read(sockfd, buffer, 8192);
    serverResponseMsg.ParseFromString(buffer);
    cout << "Server response: " << endl;
    cout << "Option: " << serverResponseMsg.option() << endl;
    cout << "Message:" << serverResponseMsg.broadcastresponse().messagestatus() << endl;
    if(serverResponseMsg.option() == 3 && serverResponseMsg.has_error()){
        cout << "Server responde with an error" << serverResponseMsg.error().errormessage() << endl;
    }

}

void *options_thread(void *args)
{
    int option;
    char buffer[1024];
    int socketFd = *(int *)args;

    while (1)
    {
        printf("4. Broadcast\n");
        cin >> option;
        if(option == 4){
            broadCast(buffer, socketFd);
        }else if(option == 1){
            
        } else {
            break;
        }
        
    }
    
}

void synchUser(struct sockaddr_in serv_addr, int sockfd, char buffer[], char *argv[]){
    int n;
    MyInfoSynchronize *clientInfo = new MyInfoSynchronize();
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
    bzero(buffer,8192);
    n = read(sockfd, buffer, 255);
    if (n < 0) 
         error("ERROR reading from socket");

        //read server response
    ServerMessage serverResponseMsg;
    serverResponseMsg.ParseFromString(buffer);
    cout << "Server response: " << endl;
    cout << "Option: " << serverResponseMsg.option() << endl;
    cout << "User Id: " << serverResponseMsg.myinforesponse().userid() << endl;
    
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
}

int main(int argc, char *argv[])
{
    GOOGLE_PROTOBUF_VERIFY_VERSION;
    int sockfd, portno, n;
    struct sockaddr_in serv_addr;
    struct hostent *server;
    int option;


    char buffer[8192];
    
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

    synchUser(serv_addr, sockfd, buffer, argv);

    pthread_t listen_client;
    pthread_t options_client;

    if (pthread_create(&listen_client, NULL, listen_thread, (void *)&sockfd) 
            || pthread_create(&options_client, NULL, options_thread, (void *)&sockfd))
            {
                cout << "Error: unable to create threads." << endl;
                exit(-1);
            }

            pthread_join (listen_client, NULL);
            pthread_join (options_client, NULL);
    

    
    close(sockfd);
    return 0;
}