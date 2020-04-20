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
You need to have Node.js and npm in your computer. 

-> For Windows users [download](https://nodejs.org/en/#home-downloadhead) the package manager <br />
-> For macOS users [download](https://nodejs.org/en/download/) the installer <br />
-> For Linux users [download](https://nodejs.org/en/download/) 

# Installing
1. Clone the repository using `https://github.com/diazMafer/Embacy.io-Clone`
2. Move to the directory where you clone the repository <br />
3. Run `yarn` or `npm install` to install dependencies.<br />
4. Run `npm run start` to see the example app at `http://localhost:8080`.

## Commands
- `npm run start` - start the dev server
- `npm run build` - create a production ready build in `dist` folder

## Built with
This project features this tools

- ⚛ **[React](https://reactjs.org)** 
- 🛠 **[Babel](https://babeljs.io/)** — ES6 syntax, Airbnb & React/Recommended config
- 🚀 **[Webpack](https://webpack.js.org/)**  — Hot Reloading, Code Splitting, Optimized Build
- 💅 **[CSS](https://postcss.org/)** — Styled Components
- 💖  **[Lint](https://eslint.org/docs/user-guide/getting-started)** — ESlint

## Authors
* **María Fernanda López Díaz** - *Initial work* - [diazMafer](https://github.com/diazMafer)
