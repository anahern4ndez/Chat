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
#include "mensaje.pb.h"
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
                    cout << "Message received from user with ID " << serverMessage.broadcast().userid() << endl;
                    cout << serverMessage.broadcast().username() << ": " << serverMessage.broadcast().message().c_str() << endl;
                } else if(serverMessage.broadcast().has_userid()){
                    cout << "Message received from user with ID " << serverMessage.broadcast().userid() << endl;
                    cout << "\t --> " << serverMessage.broadcast().message().c_str() << endl;
                } else {
                    cout << "No username or userid sent by server" << endl;
                }

            }
            else if (serverMessage.option() == ServerOpt::BROADCAST_RESPONSE)
            {
                cout << "Server response: " << endl;
                cout << "Option: " << serverMessage.option() << endl;
                cout << "Message:" << serverMessage.broadcastresponse().messagestatus() << endl;

            } else if (serverMessage.option() == ServerOpt::CHANGE_STATUS)
            {
                cout << "Server change status correctly" << endl;
    
            } 
            else if(serverMessage.option() == ServerOpt::DM_RESPONSE){
                if(serverMessage.directmessageresponse().messagestatus() == "SENT"){
                    cout << "Message sent successfully!" << endl;
                }
                else
                    cout << "Failed to send message." << endl;
            } else if (serverMessage.option() == ServerOpt::MESSAGE)
            {
                if(serverMessage.message().has_username()){
                    cout << serverMessage.message().username() << ": " << serverMessage.message.message.c_str() << endl;
                } else {
                    cout << "Message received from user with ID " <<serverMessage.message().userid() << endl;
                    cout << "\t --> " << serverMessage.message().message().c_str() << endl;
                }
            } else if (serverMessage.option() == ServerOpt::C_USERS_RESPONSE)
            {
                if((serverMessage.connecteduserresponse().connectedusers_size()) != 0){
                    cout << "Users connected in chat are: " << endl;
                    cout << "Users connected: " << serverMessage.connecteduserresponse().connectedusers_size() << endl;
                    for (int i = 0; i < serverMessage.connecteduserresponse().connectedusers_size(); i++) {
                        ConnectedUser tmpUser = serverMessage.connecteduserresponse().connectedusers(i);
                        cout << "USERNAME: " << tmpUser.username() << endl;
                        cout << "----------------------------------" << endl;
                    }
                } else {
                    cout << "No users connected in the chat" << endl;
                }

            } else if (serverMessage.option() == ServerOpt::ERROR && serverMessage.has_error())
            {
                cout << "Server response with an error: " << serverMessage.error().errormessage() << endl;
                goto loop;
            }
            serverMessage.Clear();
        }
        else {
            pthread_cancel(options_client); //request para que el otro thread termine
            std::cout << "exiting" << endl;
            // close(socketFd);
            exit(0);
            pthread_exit(0);
        }
    }
    pthread_exit(0);
}

string getUsername(string input){
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

void broadCast(char buffer[], int sockfd, string message){
    string binary;
    ClientMessage clientMessage;
    ServerMessage serverResponseMsg;
    BroadcastRequest *brdRequest = new BroadcastRequest();
    clientMessage.set_option(ClientOpt::BROADCAST_C);
    brdRequest->set_message(message);
    clientMessage.set_allocated_broadcast(brdRequest);
    clientMessage.SerializeToString(&binary);
    // sending clientMessage to server
    char cstr[binary.size() + 1];
    strcpy(cstr, binary.c_str());
    // send to socket
    send(sockfd, cstr, strlen(cstr), 0 );
    cout << "Sending broadcast request to server" <<endl;
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
    // sending clientMessage to server
    char cstr[binary.size() + 1];
    strcpy(cstr, binary.c_str());
    // send to socket
    send(sockfd, cstr, strlen(cstr), 0 );
    printf("Sending DM to %s by request to server\n", recipient_username.c_str());
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

    char cstr[binary.size() + 1];
    strcpy(cstr, binary.c_str());

    send(sockfd, cstr, strlen(cstr), 0);
    printf("Sending change status request to server\n");

}

void connectedUsers(char buffer[], int socketfd){
    string binary;
    ClientMessage ClientMessage;

    connectedUserRequest *usersRequest = new connectedUserRequest();
    ClientMessage.set_option(ClientOpt::CONNECTED_USERS);
    usersRequest->set_userid(0);
    ClientMessage.set_allocated_connectedusers(usersRequest);
    ClientMessage.SerializeToString(&binary);
    char cstr[binary.size() + 1];
    strcpy(cstr, binary.c_str());
    send(socketfd, cstr, strlen(cstr), 0);
    printf("Sending connected user Request to server \n");
}

void requestUserIfo(int socketfd, string username){
    string binary;
    ClientMessage ClientMessage;

    connectedUserRequest *usersRequest = new connectedUserRequest();
    ClientMessage.set_option(ClientOpt::CONNECTED_USERS);
    usersRequest->set_username(username);
    ClientMessage.set_allocated_connectedusers(usersRequest);
    ClientMessage.SerializeToString(&binary);
    char cstr[binary.size() + 1];
    strcpy(cstr, binary.c_str());
    send(socketfd, cstr, strlen(cstr), 0);
    printf("Sending connected user Request to server \n");
    
}

void *options_thread(void *args)
{
    int option;
    char buffer[MAX_BUFFER];
    int socketFd = *(int *)args;
    int status;
    string newStatus;
    string message, directMessage, recipient_username;
    int idDestinatary;
    printf("Thread for sending requests to server created\n");

    while (1)
    {
        printf("1.. Broadcast\n");
        printf("2. Direct Message\n");
        printf("3. Change Status\n");
        printf("4. See ConnectedUsers \n");
        printf("5. Request Information of a User\n");
        printf("6. Info\n");
        printf("7. Exit\n");
        cin >> option;
        if (option == 1){
            cin.ignore();
            printf("Enter the message you want to send: ");
            std::getline(cin, message);
            broadCast(buffer, socketFd, message);
            sleep(3);
        } else if (option == 3){
            cin.ignore();
            do {
                printf("Escoge un estado\n");
                printf("1. Activo\n");
                printf("2. Ocupado\n");
                printf("3. Inactivo\n");
                cin >> status;
                switch (status)
                {
                case 1:
                    newStatus = "Activo";
                    changeStatus(newStatus, socketFd);
                    status = -1;
                    break;
                case 2:
                    newStatus = "Ocupado";
                    changeStatus(newStatus, socketFd);
                    status = -1;
                    break;
                case 3:
                    newStatus = "Inactivo";
                    changeStatus(newStatus, socketFd);
                    status = -1;
                    break;
                default:
                    printf("Bad Option\n");
                    break;
                }

            } while (status != -1);
            sleep(3);

        } else if (option == 2){
            cin.ignore();
            printf("Users Available in Chat: \n");
            connectedUsers(buffer, socketFd);
            sleep(5);
            printf("Type your message in the format <username> <message> ");
            getline(cin, directMessage);
            recipient_username = getUsername(directMessage);
            string message = getMessage(directMessage, recipient_username);

            directMS(socketFd, message, recipient_username);
            sleep(3);
        } 
        else if (option == 7){
            memset(&buffer[0], 0, sizeof(buffer)); //clear buffer
            send(socketFd, NULL, 0, 0); //enviar mensaje vacio para notificar al servidor
            pthread_cancel(listen_client); //request para que el otro thread termine
            cout << "\nGoodbye!" << endl;
            close(socketFd);
            pthread_exit(0);
            sleep(2);
        } else if (option == 4){
            connectedUsers(buffer, socketFd);
            sleep(5);
        } else if (option == 5){
            string username;
            connectedUsers(buffer, socketFd);
            sleep(5);
            printf("Enter the username of the user you want to request information: ");
            cin >> username;
            requestUserIfo(socketFd, username);
        } else {
            cout << "Invalid option. " << endl;
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
    clientMessage.set_option(ClientOpt::SYNC);
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

    synchUser(serv_addr, sockfd, buffer, argv);
    if (pthread_create(&listen_client, NULL, listen_thread, (void *)&sockfd) || pthread_create(&options_client, NULL, options_thread, (void *)&sockfd))
    {
        cout << "Error: unable to create threads." << endl;
        exit(-1);
    }
    
    pthread_join (listen_client, NULL);
    pthread_join (options_client, NULL);
    return 0;
}