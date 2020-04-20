#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <pthread.h>
#include <algorithm>
#include <sstream>
#include <vector>
#include <errno.h> 
#include <netdb.h>  
#include <arpa/inet.h> 
#include "mensaje.pb.h"

# define DEFAULT "\033[0m"
# define MAGENTA "\033[35m"      
# define RED     "\033[31m"    
# define BLUE    "\033[34m"      
# define GREEN   "\033[32m"      
# define YELLOW  "\033[33m"      
# define BOLDBLACK   "\033[1m\033[30m"          
 
using namespace std;
using namespace chat;

#define MAX_BUFFER 8192//tamaño maximo de caracteres para mandar mensaje

vector<int> chatsDirectos;

pthread_t listen_client;
pthread_t options_client;


// opciones de mensaje para el cliente
enum ClientOpt {
    SYNC = 1,
    CONNECTED_USERS = 2,
    STATUS = 3,
    BROADCAST_C =4,
    DM = 5
};
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
    char buffer[MAX_BUFFER];
    string msgSerialized;
    ClientMessage clientMessage;
    ClientMessage clientAcknowledge;
    ServerMessage serverMessage;

    cout << "Thread for hearing responses of server created" << endl;

    loop: while (1)
    {
        int bytes_received = recv(socketFd, buffer, MAX_BUFFER, 0);
        // recepcion y parse de mensaje del server
        if (bytes_received > 0){
            serverMessage.ParseFromString(buffer);
            
            if(serverMessage.option() == ServerOpt::BROADCAST_S){
                if(serverMessage.broadcast().has_username()){
                    cout << "From: " << BLUE << serverMessage.broadcast().username() << DEFAULT << GREEN << ". To: Everyone" << DEFAULT << endl;
                    cout << "\t" << serverMessage.broadcast().message().c_str() << endl;
                } else if(serverMessage.broadcast().has_userid()){
                    cout << "From: " << BLUE << serverMessage.broadcast().userid() << DEFAULT << GREEN << ". To: Everyone" << DEFAULT << endl;
                    cout << "\t" << serverMessage.broadcast().message().c_str() << endl;
                } else {
                    cout << RED << "No username or userid sent by server" << DEFAULT << endl;
                }

            }
            else if (serverMessage.option() == ServerOpt::BROADCAST_RESPONSE)
            {
                cout << BOLDBLACK << "Server response: " << serverMessage.broadcastresponse().messagestatus() << DEFAULT << endl;
               
            } else if (serverMessage.option() == ServerOpt::CHANGE_STATUS)
            {
                cout << BOLDBLACK << "Server change status correctly" << DEFAULT << endl;
    
            } 
            else if(serverMessage.option() == ServerOpt::DM_RESPONSE){
                if(serverMessage.directmessageresponse().messagestatus() == "SENT" || serverMessage.directmessageresponse().has_messagestatus())
                    cout << MAGENTA << "Message sent successfully!" << DEFAULT << endl;
                else
                    cout << RED << "Failed to send message." << endl;
            } else if (serverMessage.option() == ServerOpt::MESSAGE)
            {
                if(serverMessage.message().has_username()){
                    cout << "From: " << BLUE << 
                    serverMessage.message().username() << DEFAULT << YELLOW << " (In Private): " << DEFAULT << endl;
                    cout << "\t" << serverMessage.message().message().c_str() << endl;
                } else {
                    cout << "From: " << BLUE << serverMessage.message().userid() << DEFAULT << YELLOW << " (In Private):" << DEFAULT << endl;
                    cout << "\t" << serverMessage.message().message().c_str() << endl;
                }
            } else if (serverMessage.option() == ServerOpt::C_USERS_RESPONSE)
            {
                
                if((serverMessage.connecteduserresponse().connectedusers_size()) != 0){
                    cout << BOLDBLACK << "Users connected in chat are: " << DEFAULT << endl;
                    cout << "Users connected: " << serverMessage.connecteduserresponse().connectedusers_size() << endl;
                    for (int i = 0; i < serverMessage.connecteduserresponse().connectedusers_size(); i++) {
                        ConnectedUser tmpUser = serverMessage.connecteduserresponse().connectedusers(i);
                        cout << "USERNAME: " << tmpUser.username() << endl;
                        if(tmpUser.has_userid())
                             cout << "ID: " << tmpUser.userid() << endl;
                        if(tmpUser.has_status())    
                            cout << "STATUS: " << tmpUser.status() << endl;
                        if(tmpUser.has_ip())    
                            cout << "IP: " << tmpUser.ip() << endl;
                        cout << "----------------------------------" << endl;
                    }
          
                } else {
                    cout << RED << "No users connected in the chat" << DEFAULT << endl;
                }

            } else if (serverMessage.option() == ServerOpt::ERROR && serverMessage.has_error())
            {
                cout << RED << "Server response with an error: " << serverMessage.error().errormessage() << DEFAULT << endl;
                if(serverMessage.error().errormessage() == "Failed to Synchronize" || serverMessage.error().errormessage() == "Failed to Acknowledge" )
                    exit(0);
                else
                    goto loop;
            }
            serverMessage.Clear();
        }
        else {
            pthread_cancel(options_client); //request para que el otro thread termine
            std::cout << "Exiting client." << endl;
            // close(socketFd);
            exit(0);
            pthread_exit(0);
        }
    }
    pthread_exit(0);
}

string getFirst(string input){
    istringstream inputStream(input);
    string username;
    inputStream >> username;
    return username;
}

string getMessage(string input, string toErase) {
    size_t pos = input.find(toErase);

    if (pos != string::npos) 
        input.erase (pos, toErase.length() + 1);

    return input;
}

void sendBySocket(string message, int sockfd){
    char cstr[message.size() + 1];
    strcpy(cstr, message.c_str());
    // send to socket
    send(sockfd, cstr, strlen(cstr), 0 );
}

void broadCast(char buffer[], int sockfd, string message){
    string binary;
    ClientMessage clientMessage;
    ServerMessage serverResponseMsg;
    BroadcastRequest *brdRequest = new BroadcastRequest();
    clientMessage.set_option(ClientOpt::BROADCAST_C);
    brdRequest->set_message(message);
    clientMessage.set_allocated_broadcast(brdRequest);
    clientMessage.SerializeToString(&binary);
    sendBySocket(binary, sockfd);
    cout << BOLDBLACK << "Sending broadcast request to server" << DEFAULT << endl;
    cout << endl;

}

void directMS(int sockfd, string message, string recipient_username){
    string binary;
    ClientMessage clientMessage;
    DirectMessageRequest *directMsRequest = new DirectMessageRequest();
    directMsRequest->set_message(message);
    directMsRequest->set_username(recipient_username);
    clientMessage.set_option(ClientOpt::DM);
    clientMessage.set_userid(sockfd);
    clientMessage.set_allocated_directmessage(directMsRequest);
    clientMessage.SerializeToString(&binary);
    sendBySocket(binary, sockfd);
    cout << BOLDBLACK << "Sending DM to:" << DEFAULT << BLUE << recipient_username.c_str() << DEFAULT << endl;
    cout << endl;

}

void changeStatus(string status, int sockfd){
    string binary;
    ClientMessage clientMessage;
    ServerMessage serverResponseMsg;


    ChangeStatusRequest *statusRequest = new ChangeStatusRequest();
    clientMessage.set_option(ClientOpt::STATUS);
    statusRequest->set_status(status);


    clientMessage.set_allocated_changestatus(statusRequest);
    clientMessage.SerializeToString(&binary);

    sendBySocket(binary, sockfd);
    cout << BOLDBLACK << "Sending change status request to server" << DEFAULT << endl;
    cout << endl;

}

void connectedUsers(char buffer[], int socketfd){
    string binary;
    ClientMessage ClientMessage;

    connectedUserRequest *usersRequest = new connectedUserRequest();
    ClientMessage.set_option(ClientOpt::CONNECTED_USERS);
    usersRequest->set_userid(0);
    ClientMessage.set_allocated_connectedusers(usersRequest);
    ClientMessage.SerializeToString(&binary);
    sendBySocket(binary, socketfd);
    cout << BOLDBLACK << "Sending connected user Request to server" << DEFAULT << endl;
    cout << endl;
    
}

void requestUserIfo(int socketfd, string username){
    string binary;
    ClientMessage ClientMessage;

    connectedUserRequest *usersRequest = new connectedUserRequest();
    ClientMessage.set_option(ClientOpt::CONNECTED_USERS);
    usersRequest->set_username(username);
    ClientMessage.set_allocated_connectedusers(usersRequest);
    ClientMessage.SerializeToString(&binary);
    sendBySocket(binary, socketfd);
    cout << BOLDBLACK << "Sending connected user Request to server" << DEFAULT << endl;
    cout << endl;
    
}

void *options_thread(void *args)
{
    string input;
    char buffer[MAX_BUFFER];
    int socketFd = *(int *)args;
    int status;
    string newStatus;
    string message, directMessage, recipient_username;
    int idDestinatary;
    printf("Thread for sending requests to server created\n");
    cout<<endl;
    cout << endl;
    cout << "* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *" << endl;
    cout << "* To send a broadcast message type: 'broadcast <yourmessage>'   *" << endl;
    cout << "* To change status type: 'status <newstatus>'                   *" << endl;
    cout << "* To see all users connected type: 'users'                      *" << endl;
    cout << "* To see all the information of a user type: '<username>'       *" << endl;
    cout << "* To send a direct message type: '<username> <yourmessage>'     *" << endl;
    cout << "* To see information type: 'info'                               *" << endl;
    cout << "* To exit type: 'exit'                                          *" << endl; 
    cout << "* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * " << endl;
    cout << endl;

    while (1)
    {

        getline(cin, input);

        string action = getFirst(input);
        string message = getMessage(input, action);

        if (action == "broadcast"){
            broadCast(buffer, socketFd, message);
            sleep(3);
        } else if (action == "status"){
            changeStatus(message, socketFd);
            sleep(3);
        } else if (action == "exit"){
            memset(&buffer[0], 0, sizeof(buffer)); //clear buffer
            send(socketFd, NULL, 0, 0); //enviar mensaje vacio para notificar al servidor
            pthread_cancel(listen_client); //request para que el otro thread termine
            cout << "\nGoodbye!" << endl;
            close(socketFd);
            pthread_exit(0);
            sleep(2);
        } else if (action == "users"){
            // printf("entro");
            connectedUsers(buffer, socketFd);
            sleep(5);
        } else if (action == "info" || action == ""){
            cout << endl;
            cout << "* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *" << endl;
            cout << "* To send a broadcast message type: 'broadcast <yourmessage>'   *" << endl;
            cout << "* To change status type: 'status <newstatus>'                   *" << endl;
            cout << "* To see all users connected type: 'users'                      *" << endl;
            cout << "* To see all the information of a user type: '<username>'       *" << endl;
            cout << "* To send a direct message type: '<username> <yourmessage>'     *" << endl;
            cout << "* To see information type: 'info'                               *" << endl;
            cout << "* To exit type: 'exit'                                          *" << endl; 
            cout << "* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * " << endl;
            cout << endl;

        } else {
            if(message == ""){
                requestUserIfo(socketFd, action);
            } else {
                directMS(socketFd, message, action);
                sleep(3);
            }
            
        }
        
    }
    
}

void synchUser(struct sockaddr_in serv_addr, int sockfd, char buffer[], string ip, char *argv[]){
    int n;
    MyInfoSynchronize *clientInfo = new MyInfoSynchronize();
    clientInfo->set_username(argv[1]);
    clientInfo->set_ip(ip);
    // Se crea instancia de Mensaje, se setea los valores deseados
    ClientMessage clientMessage;
    clientMessage.set_option(ClientOpt::SYNC);
    clientMessage.set_userid(sockfd);
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
    bzero(buffer,MAX_BUFFER);
    n = read(sockfd, buffer, 255);
    if (n < 0) 
         error("ERROR reading from socket");

        //read server response
    ServerMessage serverResponseMsg;
    serverResponseMsg.ParseFromString(buffer);
    if(serverResponseMsg.option() == ServerOpt::ERROR){
        cout << RED << "Failed to establish connection to server. Exiting." << DEFAULT << endl;
        exit(0);
    }
    cout << "Server response: " << endl;
    cout << "Option: " << serverResponseMsg.option() << endl;
    cout << "User Id: " << serverResponseMsg.myinforesponse().userid() << endl;
    
    // client response (acknowledge)
    MyInfoAcknowledge * infoAck(new MyInfoAcknowledge);
    infoAck->set_userid(sockfd);
    ClientMessage clientAcknowledge; //no se como hacerle "clear" al clientmessage entonces creare otro :( 
    clientAcknowledge.set_option(ClientOpt::SYNC);
    clientAcknowledge.set_userid(sockfd); //hay que cambiarlo para que sea dinamico
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
    char buffer[MAX_BUFFER];
    
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
    string ip(inet_ntoa(*((struct in_addr*) server->h_addr_list[0])));
    synchUser(serv_addr, sockfd, buffer, ip, argv);
    if (pthread_create(&listen_client, NULL, listen_thread, (void *)&sockfd) || pthread_create(&options_client, NULL, options_thread, (void *)&sockfd))
    {
        cout << RED << "Error: unable to create threads." << DEFAULT << endl;
        exit(-1);
    }
    
    pthread_join (listen_client, NULL);
    pthread_join (options_client, NULL);
    return 0;
}