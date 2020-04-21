/* 
 * Client to connect with server using multithreading for hearing and sending request
 * was made for a Chat project for 
 * version: 20/04/2019
 * Authors: Maria F. Lopez, Ana Lucia Hernandez, David Soto
*/


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

#define MAX_BUFFER 8192 //max buffer size for sending and receiving messages from/to server

pthread_t listen_client;
pthread_t options_client;

//options of requests client can make to server
enum ClientOpt {
    SYNC = 1,
    CONNECTED_USERS = 2,
    STATUS = 3,
    BROADCAST_C =4,
    DM = 5,
    ACKNOWLEDGE = 6
};

//options of responses from server to client
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

/*
 * Method to send error message to client and exit program
 * params: char *msg - message to show 
*/
void error(const char *msg)
{
    perror(msg);
    exit(0);
}

/*
 * Thread for hearing responses of server, while socket is open 
 * the client will hear for each response and show the client
 * specific information depending on each Server Option
 * params: int - socket
*/
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
        // reception and parse for server messages
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

/*
 * Method for spliting input of client in two
 * params: input - whole input of client
 * return: first word before a blank space
*/
string getFirst(string input){
    istringstream inputStream(input);
    string username;
    inputStream >> username;
    return username;
}

/*
 * Method for spliting input of client in two
 * params: input - whole input of client, toErase - first word before a blank space
 * return: second word, first word after a blank space
*/

string getMessage(string input, string toErase) {
    size_t pos = input.find(toErase);

    if (pos != string::npos) 
        input.erase (pos, toErase.length() + 1);

    return input;
}

/*
 * Method for sending request to server
 * params: message - serialized message , int - socket
*/
void sendBySocket(string message, int sockfd){
    char cstr[message.size() + 1];
    strcpy(cstr, message.c_str());
    // send to socket
    send(sockfd, cstr, strlen(cstr), 0 );
}

/*
 * Method to send a broadcast request to server 
 * a ClientMessage variable is fill up with a BROADCAST REQUEST option 
 * the BroadCastRequest is fill up with the message the user wants to send
 * finally is send by the socket to the server
 * params: int - socket, string - message user wants to send
*/
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

/*
 * Method to send a direct message request to server 
 * a ClientMessage variable is fill up with a DIRECT MESSAGE REQUEST option, 
 * also with the user id of the user that's sending the message in this case
 * the user id is the same as the socket
 * the DirectMessageRequest is fill up with the message the user wants to send,
 * and the recipient username he wants the server to send the message
 * finally is send by the socket to the server
 * params: int - socket, string - message user wants to send, recipient_username - username of destinatiry
*/
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

/*
 * Method to send a change status request to server
 * a ClientMessage variable if fill up with a CHANGE STATUS REQUEST opton
 * the ChangeStatusRequest is fill up with the new status of the client
 * params: status - newStatus, socket - socket int
*/
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

/*
 * Method to send a request to see all users to server
 * a ClientMessage variable if fill up with a CONNECTED USER opton
 * the connctedUserRequest is fill a user id of 0 that stands for 
 * the option to get all users connected to server username
 * params: int socket
*/
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

/*
 * Method to send a request to see all users to server
 * a ClientMessage variable if fill up with a CONNECTED USER opton
 * the connctedUserRequest is fill a user id of 0 that stands for 
 * the option to get all users connected to server username
 * params: int socket
*/
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

/*
 * Thread for sending requests to server while socket is open
 * the way the user selects an option is by writing it as the 
 * firts word of his whole inppu
 * params: int socket
*/
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
            send(socketFd, NULL, 0, 0); //send the message to notify the server
            pthread_cancel(listen_client); //request to ask the listen thread to finish 
            cout << "\nGoodbye!" << endl;
            close(socketFd);
            pthread_exit(0);
            sleep(2);
        } else if (action == "users"){
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

/*
 * Method to synch user with the server 
 * the username, ip and socket are send to server 
*/

void synchUser(struct sockaddr_in serv_addr, int sockfd, char buffer[], string ip, char *argv[]){
    int n;
    MyInfoSynchronize *clientInfo = new MyInfoSynchronize();
    clientInfo->set_username(argv[2]);
    clientInfo->set_ip(ip);
    ClientMessage clientMessage;
    clientMessage.set_option(ClientOpt::SYNC);
    clientMessage.set_userid(sockfd);
    clientMessage.set_allocated_synchronize(clientInfo);
    //Message is serialize
    string binary;
    clientMessage.SerializeToString(&binary);
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
    ClientMessage clientAcknowledge; 
    clientAcknowledge.set_option(ClientOpt::ACKNOWLEDGE);
    clientAcknowledge.set_userid(sockfd); 
    clientAcknowledge.set_allocated_acknowledge(infoAck);
    string binarya;
    clientAcknowledge.SerializeToString(&binarya);
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

    portno = atoi(argv[4]);
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");
    server = gethostbyname(argv[3]);
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