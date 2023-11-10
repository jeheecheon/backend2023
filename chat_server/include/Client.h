#pragma once

#ifndef USER_H
#define USER_H

#include <string>
#include <arpa/inet.h>

using namespace std;

class Room;

class Client {
public:
    Room* roomThisUserIn;  // Room 포인터
    int socketNumber;
    std::string clientName;
    string ipAddress;
    in_port_t port;

public:
    Client();
    ~Client();

    string PortAndIpIntoString();
    string PortAndIpAndNameToString();
    bool operator<(const Client& other) const;
};

#endif 
