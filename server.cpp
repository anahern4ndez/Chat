#include <iostream>
#include <fstream>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/select.h>
#include <netdb.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <arpa/inet.h>
#include <unordered_map>
#include <fcntl.h>
#include <iomanip>
#include <map>
#include "mensaje.pb.h"
using namespace chat;

#define QUEUE_MAX 10 // establece el numero maximo de conexiones en cola 
#define MAX_CONNECTIONS 20 //establece la cantidad maxima de conexiones que el server puede tener simultaneamente
#define MAX_BUFFER 8192 //cantidad maxima de caracteres en un mensaje
#define INACTIVE_TIME 20 //cantidad de tiempo sin actividad para cambiar de estado a cliente 

// Con esto podremos manejar los tiempos de cada usuarios que ingrese
std::map<int, float> tiemposInactivos;

// opciones de mensaje para el cliente
enum ClientOpt {
    SYNC = 1,
    CONNECTED_USERS = 2,
    STATUS = 3,
    BROADCAST_C =4,
    DM = 5,
    ACKNOWLEDGE = 6
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
/*
    Estructura de queues para el envio de mensajes. 
*/
typedef struct {
    char *buffer[MAX_BUFFER];
    int head, tail;
    int full, empty;
    pthread_mutex_t *mutex;
    pthread_cond_t *notFull, *notEmpty;
} message_queue;

/*
    En este struct se guardan las queues y threads necesarios para el manejo de clientes
*/
typedef struct {
    int socketFd; // file descriptor del main socket donde el server esta escuchando
    int connected_clients[MAX_CONNECTIONS]; //lista donde se guardan los FDs de los sockets de clientes conectados
    pthread_t threads[MAX_CONNECTIONS];
    pthread_mutex_t client_queue_mutex; //lock para poder modificar la lista de clientes conectados
    int client_num; //cantidad de clientes conectados
    message_queue *broadcast_messages; // queue se usara para cuando se quiera hacer un broadcast
    fd_set all_sockets; // pool de socketFds aceptados
} chat_data;


 /*
    Struct que contiene toda la información relevante del cliente. 
 */
struct Client
{
    int socketFd;
    std::string username;
    int userid;
    char ip_address[INET_ADDRSTRLEN];
    std::string status;
    message_queue *received_messages;
    message_queue *sent_messages;
};

bool newRequest;
bool changeStatus;

struct thread_params { chat_data *c_data; struct sockaddr_in *cli_addr; };

std::unordered_map<std::string, Client *> clients;

//se nombrarán las funciones aquí pero las implementaciones están de último
void bind_socket(struct sockaddr_in serverAddr, int socketFd, long port);
void *client_thread(void *params);
void ErrorToClient(int socketFd, std::string errorMsg);
message_queue* init_queue(void);
void init_chat(int sockfd);
void new_client(chat_data *chat, int new_socket);
std::string find_by_id(int id, std::string sender_username);
void *listen_to_connections(void *params);
void *timer(void *params);

void error(const char *msg)
{
    perror(msg);
    exit(1);
}


int main(int argc, char *argv[])
{
    GOOGLE_PROTOBUF_VERIFY_VERSION;
    int sockfd, client_num;
    sockaddr_in serv_addr, cli_addr;
    long port = 9999; // el puerto default es 9999
    char cli_addr_addr[INET_ADDRSTRLEN];
    pthread_t messagesThread;
    chat_data data;
    if (argc == 2)
    {
        port = atoi(argv[1]);
    }
    std::cout << "**** Si desea salir, ingresar 'exit' ****" << std::endl;
    /* 
        La siguiente conexión y binding a un socket se tomó de:
        https://www.bogotobogo.com/cplusplus/sockets_server_client.php
    */
    // create a socket
    sockfd =  socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
    error("ERROR opening socket");    
    // clear address structure
    bzero((char *) &serv_addr, sizeof(serv_addr));
    bind_socket(serv_addr, sockfd, port);
    // inicializar todas las variables del struct de chat
    data.socketFd = sockfd;
    data.broadcast_messages = init_queue();
    data.client_num = 0;
    FD_ZERO(&(data.all_sockets));
    FD_SET(sockfd, &(data.all_sockets));
    pthread_mutex_init(&data.client_queue_mutex, NULL);
    // // Se creara un nuevo thread para estar al tanto de recibir y mandar mensajes
    // // el thread actual (padre) quedara para escuchar nuevas conexiones 
    // El socket actual queda abierto para nuevas conexiones, hasta que se les haga accept() quedan en cola, 
    // el número máximo de elementos en cola es QUEUE_MAX.
    if (listen(sockfd, QUEUE_MAX) == -1)
    {
        close(sockfd);
        error("ERROR on listening.\n");
    }
    printf("Listening...  %ld\n", port);
    thread_params params = {&data, &cli_addr};
    pthread_t listen_c_t;
    pthread_create(&listen_c_t, NULL, listen_to_connections, (void *)&params);
    std::string exit_v;
    while(exit_v != "exit"){
        std::cin >> exit_v;
    }
    std::cout << "Ending all connections... " << std::endl;
    //notify connected clients 
    for (auto user = clients.begin(); user != clients.end(); ++user){
        send(user->second->socketFd, NULL, 0, 0);
    }
    //close all sockets & end all threads
    pthread_cancel(listen_c_t);
    for(int i = 0; i< data.client_num; i++){
        close(data.connected_clients[i]);
        pthread_cancel(data.threads[i]);
    }
    std::cout << "Goodbye!" << std::endl;
    close(sockfd);
    pthread_mutex_destroy(&data.client_queue_mutex);
    google::protobuf::ShutdownProtobufLibrary();
    return 0;
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

void *listen_to_connections(void *params){
    thread_params *data = (thread_params *)params;
    socklen_t clilen;
    int newsockfd;
    while (1)
    {
        // el accept accederá a una nueva conexión con un cliente. Crea un nuevo socket para que el anterior 
        // pueda quedarse escuchando para otras nuevas conexiones. 
        clilen = sizeof data->cli_addr;
        newsockfd = accept(data->c_data->socketFd, (struct sockaddr *)&data->cli_addr, &clilen);
        if (newsockfd < 0)
            error("ERROR en accept()");
        new_client(data->c_data, newsockfd);
    }
    for (int i = 0; i < data->c_data->client_num; i++)
    {
        pthread_join(data->c_data->threads[i], NULL);
    }
    
}

void bind_socket(struct sockaddr_in serverAddr, int socketFd, long port){
    serverAddr.sin_family = AF_INET;  
    serverAddr.sin_addr.s_addr = INADDR_ANY;  
    serverAddr.sin_port = htons(port);
    memset(serverAddr.sin_zero, 0, sizeof serverAddr.sin_zero);
    // This bind() call will bind  the socket to the current IP address on port
    if (bind(socketFd, (struct sockaddr *) &serverAddr, sizeof(serverAddr)) < 0) {
            close(socketFd);
            error("ERROR on binding\n");
    }
}

void new_client(chat_data *chat, int new_socket){
    fprintf(stderr, "Server accepted new client. Socket: %d\n", new_socket);
    pthread_mutex_lock(&chat->client_queue_mutex);
    pthread_t thread_cli;
    if(chat->client_num < MAX_CONNECTIONS){
        //revisa en toda la lista de sockets y, si no existe el que quiere setear, lo setea
        for(int i =0; i < MAX_CONNECTIONS; i++){
            if(!FD_ISSET(chat->connected_clients[i], &(chat->all_sockets)))
            {
                chat->connected_clients[i] = new_socket;
                i = MAX_BUFFER; //break
            }
        }
        FD_SET(new_socket, &(chat->all_sockets));
        chat->client_num++;
        //iniciar thread
        pthread_create(&chat->threads[chat->client_num], NULL, client_thread, (void *)&new_socket);
    }
    pthread_mutex_unlock(&chat->client_queue_mutex);
}

void *client_thread(void *params)
{
    int socketFd = *(int *)params;
    struct Client thisClient;
    char buffer[MAX_BUFFER];
    bool can_connect = false; // se permitira el envio de mensajes si se realizo el handshake
    thisClient.userid = socketFd;
    std::string msgSerialized;
    ClientMessage clientMessage;
    ClientMessage clientAcknowledge;
    ServerMessage serverMessage;
    int read_bytes = 0;

    loop: while (1)
    {   
        memset(&buffer[0], 0, sizeof(buffer)); //clear buffer
        msgSerialized[0] = 0; //clear serialized variable
        if ((read_bytes = (recv(socketFd, buffer, MAX_BUFFER, 0))) > 0)
        {   
            clientMessage.ParseFromString(buffer); 

            if (clientMessage.option() == ClientOpt::SYNC)
            {
                /*
                    THREE WAY HANDSHAKE
                */            
                if (!clientMessage.has_synchronize())
                {
                    ErrorToClient(socketFd, "Failed to Synchronize");
                    pthread_exit(0);
                }

                if(clients.count(clientMessage.synchronize().username()) > 0)
                {
                    std::cout << "Trying to sync a user with username duplicated. " << std::endl;
                    ErrorToClient(socketFd, "Username already exists in server.");
                    goto loop;
                }
                
                thisClient.username = clientMessage.synchronize().username();
                thisClient.status = "Activo";
                if(clientMessage.synchronize().has_ip())
                    strcpy(thisClient.ip_address, clientMessage.synchronize().ip().c_str());
                    // envio de response 
                //response build 
                MyInfoResponse * serverResponse(new MyInfoResponse); 
                serverResponse->set_userid(socketFd);

                serverMessage.Clear();
                serverMessage.set_option(ServerOpt::RESPONSE);
                serverMessage.set_allocated_myinforesponse(serverResponse);

                serverMessage.SerializeToString(&msgSerialized);
                memset(&buffer[0], 0, sizeof(buffer)); //clear buffer
                // enviar de mensaje de cliente a server
                strcpy(buffer, msgSerialized.c_str());
                send(socketFd, buffer, msgSerialized.size() + 1, 0);
                
                memset(&buffer[0], 0, sizeof(buffer)); //clear buffer
                recv(socketFd, buffer, MAX_BUFFER, 0);
                clientAcknowledge.ParseFromString(buffer);
                if(!clientAcknowledge.has_acknowledge()){
                    ErrorToClient(socketFd, "Failed to Synchronize");
                    pthread_exit(0);
                }
                thisClient.socketFd = socketFd;
                thisClient.received_messages = init_queue();
                thisClient.sent_messages = init_queue();
                thisClient.userid = clientMessage.userid();
                thisClient.status = "Activo";
                thisClient.username = clientMessage.synchronize().username();
                
                //agregar nuevo cliente al map de clientes
                std::pair<std::string, Client*> nclient (clientMessage.synchronize().username(), &thisClient);
                clients.insert(nclient);
                std::cout << "User "<< thisClient.username<< " connected."<<std::endl;
                can_connect = true;
                tiemposInactivos[thisClient.socketFd] = 0;
                pthread_t timer_thread;
                pthread_create(&timer_thread, NULL, timer, (void *)&thisClient);
                newRequest = true;

                
            } 
            else if(clientMessage.option() == ClientOpt::CONNECTED_USERS && can_connect){
                if (!clientMessage.has_connectedusers())
                {
                    ErrorToClient(socketFd, "Failed to request connected users.");
                    goto loop;
                }
                if(clientMessage.connectedusers().userid() == 0){ //si userid 0, se devuelven todos los usuarios
                    ConnectedUserResponse *response = new ConnectedUserResponse();
                    for (auto user = clients.begin(); user != clients.end(); ++user)
                    {
                            if(user->first != thisClient.username){
                                ConnectedUser *user_info =  response->add_connectedusers();
                                user_info->set_username(user->first);
                            }                        
                    }

                    serverMessage.Clear();
                    serverMessage.set_option(ServerOpt::C_USERS_RESPONSE);
                    serverMessage.set_allocated_connecteduserresponse(response);
                    serverMessage.SerializeToString(&msgSerialized);
                    // enviar de mensaje de cliente a server
                    char cstr[msgSerialized.size() + 1];
                    strcpy(cstr, msgSerialized.c_str());
                    send(socketFd, cstr, msgSerialized.size() + 1, 0);
                } else { // *********************
                    std::unordered_map<std::string, Client *>::const_iterator recipient;
                    std::string recipient_username = "";
                    if (clientMessage.connectedusers().has_username())
                        recipient_username = clientMessage.connectedusers().username();
                    else if (clientMessage.connectedusers().has_userid()) 
                        recipient_username = find_by_id(clientMessage.connectedusers().userid());
                    recipient = clients.find(recipient_username);
                    if (recipient == clients.end()){
                        ErrorToClient(socketFd, "Username not found.");
                        goto loop;
                    }
                    Client *info = recipient->second;
                    ConnectedUserResponse *response = new ConnectedUserResponse();
                    ConnectedUser *user_info =  response->add_connectedusers();
                    user_info->set_username(recipient->first);
                    user_info->set_status(info->status);
                    user_info->set_userid(info->userid);
                    user_info->set_ip(info->ip_address);
                
                    serverMessage.set_option(ServerOpt::C_USERS_RESPONSE);
                    serverMessage.set_allocated_connecteduserresponse(response);
                    serverMessage.SerializeToString(&msgSerialized);
                    // enviar de mensaje de cliente a server
                    char cstr[msgSerialized.size() + 1];
                    strcpy(cstr, msgSerialized.c_str());
                    send(socketFd, cstr, msgSerialized.size() + 1, 0);
                }
                newRequest = true;
                tiemposInactivos[thisClient.socketFd] = 0;

            }
            else if(clientMessage.option() == ClientOpt::STATUS  && can_connect){
                if (!clientMessage.has_changestatus())
                {
                    ErrorToClient(socketFd, "No Change Status Information sent by client");
                    goto loop;
                }

                ChangeStatusRequest statusReq = clientMessage.changestatus();
                std::cout << "Change Status Request for:" << thisClient.username << ". New status: " << statusReq.status() << std::endl;
                std::string new_status = statusReq.status();

                ChangeStatusResponse *response = new ChangeStatusResponse();
                response->set_userid(thisClient.userid);
                response->set_status(new_status);

                thisClient.status = new_status;

                serverMessage.set_option(ServerOpt::CHANGE_STATUS);
                serverMessage.set_allocated_changestatusresponse(response);
                serverMessage.SerializeToString(&msgSerialized);

                // sendig message to client
                char cstr[msgSerialized.size() + 1];
                strcpy(cstr, msgSerialized.c_str());
                send(socketFd, cstr, msgSerialized.size() + 1, 0);
                std::cout << "Server changed status for:" << thisClient.username << std::endl;
                std::cout << "Sending response to client." << std::endl;
                newRequest = true;
                tiemposInactivos[thisClient.socketFd] = 0;


            }
            else if (clientMessage.option() == ClientOpt::BROADCAST_C && can_connect){
                if (!clientMessage.has_broadcast())
                {
                    ErrorToClient(socketFd, "No Broadcast Information");
                    goto loop;
                }

                BroadcastRequest brdReq = clientMessage.broadcast();
                std::cout << "Broadcast Message Request:" << brdReq.message() << std::endl;

            
                BroadcastResponse *brdRes = new BroadcastResponse();
                brdRes->set_messagestatus("Request accepted Sending Message...");

                serverMessage.Clear();
                serverMessage.set_option(ServerOpt::BROADCAST_RESPONSE);
                serverMessage.set_allocated_broadcastresponse(brdRes);
                serverMessage.SerializeToString(&msgSerialized);

                strcpy(buffer, msgSerialized.c_str());
                send(socketFd, buffer, msgSerialized.size() + 1, 0);

        
                BroadcastMessage *brdMsg = new BroadcastMessage();
                brdMsg->set_message(brdReq.message());
                brdMsg->set_userid(socketFd);
                brdMsg->set_username(thisClient.username);

                serverMessage.Clear();
                serverMessage.set_option(ServerOpt::BROADCAST_S);
                serverMessage.set_allocated_broadcast(brdMsg);
                serverMessage.SerializeToString(&msgSerialized);

                strcpy(buffer, msgSerialized.c_str());
                for (auto item = clients.begin(); item != clients.end(); ++item)
                {
                    if (item->first != thisClient.username)
                    {
                        send(item->second->socketFd, buffer, msgSerialized.size() + 1, 0);
                        
                    }
                }
                printf("Sending Broadcast Message to all clients\n");
                newRequest = true;
                tiemposInactivos[thisClient.socketFd] = 0;

            }
            else if (clientMessage.option() == ClientOpt::DM && can_connect){
                if(!clientMessage.has_directmessage()){
                    ErrorToClient(socketFd, "Error in DM");
                    goto loop;
                }
                if(!clientMessage.directmessage().has_username() && !clientMessage.directmessage().has_userid()){
                    ErrorToClient(socketFd, "You must specify recipient's ID or username.");
                    goto loop;
                }
                std::string message_to_send = clientMessage.directmessage().message();
                std::string recipient_username = ""
                // si ha mandado el mensaje solo con el userid, encontrar el username correspodiente
                // si no, tomar el username que el cliente envio 
                if(clientMessage.directmessage().has_username())
                    recipient_username = clientMessage.directmessage().username();
                else 
                    recipient_username = find_by_id(clientMessage.directmessage().userid());
                
                std::unordered_map<std::string, Client *>::const_iterator recipient = clients.find(recipient_username);
                if (recipient == clients.end()){
                    ErrorToClient(socketFd, "Username of given UserID not found.");
                    goto loop;
                }
                // intento de enviar mensaje a recipient
                DirectMessage * dm(new DirectMessage);
                dm->set_message(message_to_send);
                dm->set_userid(socketFd);
                dm->set_username(thisClient.username);
                int recipient_fd = (recipient->second)->socketFd;
                ServerMessage to_recipient;
                to_recipient.set_option(ServerOpt::MESSAGE);
                to_recipient.set_allocated_message(dm);
                // Se serializa el message a string
                to_recipient.SerializeToString(&msgSerialized);
                char cstr[msgSerialized.size() + 1];
                strcpy(cstr, msgSerialized.c_str());   
                int success = send(recipient_fd, cstr, strlen(cstr), 0);
                if(success < 0){
                    ErrorToClient(socketFd, "Failed to send DM.");
                    goto loop;
                }
                memset(&cstr[0], 0, sizeof(cstr)); //clear buffer
                msgSerialized[0] = 0; //clear serialized variable
                // si es exitoso, mandar aviso a emisor 
                DirectMessageResponse * dm_response(new DirectMessageResponse);
                dm_response->set_messagestatus("SENT");
                ServerMessage to_sender;
                to_sender.set_option(ServerOpt::DM_RESPONSE);
                to_sender.set_allocated_directmessageresponse(dm_response);
                to_sender.SerializeToString(&msgSerialized);
                strcpy(cstr, msgSerialized.c_str());   
                send(socketFd, cstr, strlen(cstr), 0);
                newRequest = true;
                tiemposInactivos[thisClient.socketFd] = 0;

            }
            clientMessage.Clear(); // clear clientMessage
            serverMessage.Clear();
        }
        else if (read_bytes == 0){
            clients.erase(thisClient.username);
            close(socketFd);
            std::cout << "User "<< thisClient.username <<" exited. Closing socket " << socketFd << "."<< std::endl;
            pthread_exit(0);

        }
        clientMessage.Clear(); // clear clientMessage
        newRequest = false;
    }
}

std::string find_by_id(int id, std::string sender_username){
    std::string found_username = "";
    //recorrer todos los elementos del mapa y encontrar match al id
    for (auto item = clients.begin(); item != clients.end(); ++item)
    {
        if (item->first != sender_username)
        {
            if(item->second->userid == id){
                found_username = item->first;
            }
        }
    }
    return found_username;
}

message_queue* init_queue(void){
    message_queue *queue = (message_queue *)malloc(sizeof(message_queue));
    queue->empty = 1;
    queue->full = 0;
    queue->head = 0;
    queue->tail = 0;
    queue->mutex = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(queue->mutex, NULL);
    queue->notFull = (pthread_cond_t *) malloc(sizeof(pthread_cond_t));
    queue->notEmpty = (pthread_cond_t *) malloc(sizeof(pthread_cond_t));
    return queue;
}

void *timer(void *params) {
    Client client = *(Client *) params;
    ServerMessage serverMessage;
    std::string msgSerialized;


    while(1){
        if(clients.find(client.username) != clients.end()){
            if(tiemposInactivos[client.socketFd] < INACTIVE_TIME){
                sleep(1); 
                tiemposInactivos[client.socketFd]++;
                if(tiemposInactivos[client.socketFd] == INACTIVE_TIME){
                    client.status = "Inactivo";
                    ChangeStatusResponse *response = new ChangeStatusResponse();
                    response->set_userid(client.socketFd);
                    response->set_status("Inactivo");
                    serverMessage.set_option(ServerOpt::CHANGE_STATUS);
                    serverMessage.set_allocated_changestatusresponse(response);
                    serverMessage.SerializeToString(&msgSerialized);

                    // sendig message to client
                    char cstr[msgSerialized.size() + 1];
                    strcpy(cstr, msgSerialized.c_str());
                    send(client.socketFd, cstr, msgSerialized.size() + 1, 0);
                    std::cout << "Server changed status for:" << client.username << std::endl;
                    std::cout << "Sending update to client" << std::endl;
                }
            }
        }
        else {
            // std::cout << "Closing " << client.username << " timer." << std::endl;
            pthread_exit(0);
        }
    }
}