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
#include "mensaje.pb.h"
using namespace chat;

#define QUEUE_MAX 10 // establece el numero maximo de conexiones en cola 
#define MAX_CONNECTIONS 20 //establece la cantidad maxima de conexiones que el server puede tener simultaneamente
#define MAX_BUFFER 8192 //cantidad maxima de caracteres en un mensaje

/*
    Se usará una queue para monitorear a los clientes. Contiene mutex locks para cuando 
    se agrega, modifica o elimina a un cliente de a lista, y variables condición para cuando esté llena o vacía. 
*/
typedef struct {
    char *buffer[MAX_BUFFER];
    int head, tail;
    int full, empty;
    pthread_mutex_t *mutex;
    pthread_cond_t *notFull, *notEmpty;
} message_queue;

/*
    En este struct se guardan las queues y threads necesarios para el manejo 
*/
typedef struct {
    int socketFd; // file descriptor del main socket donde el server esta escuchando
    int connected_clients[MAX_CONNECTIONS]; //lista donde se guardan los FDs de los sockets de clientes conectados
    pthread_mutex_t client_queue_mutex; //lock para poder modificar la lista de clientes conectados
    int client_num; //cantidad de clientes conectados
    message_queue *broadcast_messages; // queue se usara para cuando se quiera hacer un broadcast
    fd_set all_sockets; // pool de socketFds aceptados
} chat_data;

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
    BOADCAST_RESPONSE = 7,
    DM_RESPONSE = 8
};

 /*
    Struct que contiene toda la información relevante del cliente. 
 */
struct Client
{
    int socketFd;
    std::string username;
    char ip_address[INET_ADDRSTRLEN];
    std::string status;
    message_queue *received_messages;
    message_queue *sent_messages;
};

std::unordered_map<std::string, Client *> clients;


//se nombrarán las funciones aquí pero las implementaciones están de último
void bind_socket(struct sockaddr_in serverAddr, int socketFd, long port);
void *client_thread(void *params);
void ErrorToClient(int socketFd, std::string errorMsg);
message_queue* init_queue(void);
void init_chat(int sockfd);

void error(const char *msg)
{
    perror(msg);
    exit(1);
}

chat_data data; // data global para todos los threads

int main(int argc, char *argv[])
{
    GOOGLE_PROTOBUF_VERIFY_VERSION;
    int sockfd, newsockfd, client_num;
    socklen_t clilen;
    sockaddr_in serv_addr, cli_addr;
    long port = 9999; // el puerto default es 9999
    char cli_addr_addr[INET_ADDRSTRLEN];
    pthread_t messagesThread;

    if (argc == 2)
    {
        port = atoi(argv[1]);
    }

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
    // pthread_create(&messagesThread, NULL, message_thread, (void *)&data)
    // El socket actual queda abierto para nuevas conexiones, hasta que se les haga accept() quedan en cola, 
    // el número máximo de elementos en cola es QUEUE_MAX.
    if (listen(sockfd, QUEUE_MAX) == -1)
    {
        close(sockfd);
        error("ERROR on listening.\n");
    }
    printf("Listening...  %ld\n", port);
    // Se quedara esperando nuevas conexiones 
    while (1)
    {
        // el accept accederá a una nueva conexión con un cliente. Crea un nuevo socket para que el anterior 
        // pueda quedarse escuchando para otras nuevas conexiones. 
        clilen = sizeof cli_addr;
        newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
        if (newsockfd < 0)
            error("ERROR en accept()");
        //new_client();
    }

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

void new_client(chat_data *chat, int new_socket, std::string username, char *ipAddress){
    fprintf(stderr, "Server accepted new client. Socket: %d\n", new_socket);
    pthread_mutex_lock(&chat->client_queue_mutex);
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
        struct Client newClient;
        strcpy(newClient.ip_address,ipAddress);
        newClient.socketFd = new_socket;
        newClient.username = username;
        newClient.received_messages = init_queue();
        newClient.sent_messages = init_queue();
        //agregar nuevo cliente al map de clientes
        std::pair<std::string, Client*> nclient (username, &newClient);
        clients.insert(nclient);
        //iniciar thread
        pthread_t thread_cli;
        pthread_create(&thread_cli, NULL, client_thread, (void *)&newClient);
    }
    pthread_mutex_unlock(&chat->client_queue_mutex);
}

void *client_thread(void *params)
{
    struct Client thisClient = *(Client *)params;
    int socketFd = thisClient.socketFd;
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
        if (clientMessage.option() == ClientOpt::SYNC)
        {
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


            strcpy(thisClient.ip_address, clientMessage.synchronize().ip().c_str());
            thisClient.username = clientMessage.synchronize().username();
            thisClient.socketFd = socketFd;
            clients[thisClient.username] = &thisClient;
            std::cout << "User connected"<< thisClient.username<< std::endl;
        } 
        else if (clientMessage.option() == ClientOpt::BROADCAST_C){

            printf("ENTRO \n");
            

        }
        else if (clientMessage.option() == ClientOpt::DM){
            std::string message_to_send = clientMessage.directmessage().message();

        }
        std::cout << "--- Users:  " << clients.size() << std::endl;
        clientMessage.Clear();
        
    }

    clients.erase(thisClient.username);
    close(socketFd);
  
    std::cout << "Closing socket"<< std::endl;
    pthread_exit(0);
}
/*
    Metodo del thread handler para escuchar mensajes 
*/

// void *message_thread(void *params){

// }
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