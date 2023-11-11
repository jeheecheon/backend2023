#pragma once

#ifndef USER_H
#define USER_H

#include <string>
#include <arpa/inet.h>

using namespace std;

class Room;

class User {
public:
    Room* roomThisUserIn;  // Room 포인터
    int socketNumber;
    std::string userName;
    string ipAddress;
    in_port_t port;

public:
    User();
    ~User();

    string PortAndIpIntoString();
    string PortAndIpAndNameToString();
    bool operator<(const User& other) const;
};

#endif 
