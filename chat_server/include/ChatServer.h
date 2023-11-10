// ChatServer.h
#pragma once

#ifndef CHAT_SERVER_H
#define CHAT_SERVER_H

#include <arpa/inet.h>
#include <sys/socket.h>

#include <set>
#include <thread>

#include "../include/MessageHandlers.h"
#include "../include/SmallWorkHandler.h"
#include "GlobalVariables.h"
#include "Client.h"

using namespace std;

class ChatServer {
   private:
    int _serverSocket;
    set<Client> _clients;
    int _port;
    bool _isBinded;

    std::set<std::unique_ptr<std::thread>> workers;
    // 종료할 클라이언트 소켓 set
    unordered_set<int> _willClose;

   public:
    ChatServer();
    ~ChatServer();

    bool OpenServerSocket();
    bool BindServerSocket(int port, in_addr_t addr);
    bool Start(int numOfWorkerThreads);

    bool _CustomReceive(int clientSock, void* buf, size_t size, int& numRecv);

   private:
    void Cleanup();
};

#endif  // CHAT_SERVER_H
