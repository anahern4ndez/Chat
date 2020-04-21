# Multithreaded Chat 

## Introduction
The Chat is separated by a Client and a Server. The server can connect a maximum of 20 clients.
- Server: maintains a list of all clients/users connected to the system. It uses two main threads, one thread is made for each new client for hearing requests and another one is made for hearing new connections. The Server uses a queue for broadcasting messages. The clients will connect to the server using the IP address of the machine where the server is running on the local network. 
- Client: connects and registers with the server. It has a screen that displays the messages that the user has received from the server, sent by a remote user; and in which text input is allowed to send messages to other users. It uses two main threads one for hearing server responses such as direct messages or broadcast messages and one for sending requests to the server 

A protocol was defined with objects for each option the client and server is capable for making. The options the client can make a request for are :
 1. Send a broadcast message 
 2. Send a direct message
 3. Change my status 
 4. See all users connected to the chat
 5. Request information of a user
 6. Help
 7. Exit

When the user has passed an amount of time without sending requests the server changes his status to inactive.

## Getting Started
These instructions will get you a copy of the project up and running on your local machine for development.

# Prerequisites
-> [Google Protocol Buffer](https://nodejs.org/en/#home-downloadhead) 

# Installing
1. Clone the repository using `https://github.com/her17138/Chat`
2. Compile the .proto, this will generate message.pb.cc and message.pb.h `protoc -I=. --cpp_out=. mensaje.proto`

## Compile the program with 
- `g++ -std=c++11 client.cpp mensaje.pb.cc -lprotobuf -lpthread -o client` - Compiles the client
- `g++ -std=c++11 server.cpp mensaje.pb.cc -lprotobuf -lpthread -o server` - Compiles the server

## Execute the program with
- `./client [name] [username] [ipServer] [port]`
- `./server [port]` If no port is provided the default is de 9999

## Instructions client can execute
 - To send a broadcast message type: **'broadcast yourmesssage'**   
 - To change status type: **'status newstatus'**             
 - To see all users connected type: **'users'**                      
 - To see all the information of a user type: **'username'**      
 - To send a direct message type: **'username yourmessage'**
 - To see information type: **'info'**                             
 - To exit type: **'exit'**                                    


## Authors
* **María Fernanda López Díaz** - [diazMafer](https://github.com/diazMafer)
* **Ana Lucía Hernández Ordoñez** - [her17138](https://github.com/her17138)
* **David Uriel Soto** - [DavidSoto5317](https://github.com/DavidSoto5317)
