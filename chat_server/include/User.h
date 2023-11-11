#pragma once

#ifndef USER_H
#define USER_H

#include <string>
#include <arpa/inet.h>
#include <memory>

using namespace std;

class Room;

class User {
public:
    shared_ptr<Room> roomThisUserIn;  // Room 포인터
    int socketNumber;
    string userName;
    string ipAddress;
    in_port_t port;

public:
    User();
    ~User();

    string PortAndIpIntoString() const;
    string PortAndIpAndNameToString() const;
    bool operator<(const User& other) const;
    string GetUserName() const;
};

#endif 
