#include "../include/User.h"
#include <iostream>
using namespace std;

User::User() {
    userName = "None";
    roomThisUserIn = nullptr;
}

User::~User() {}

string User::PortAndIpIntoString() const {
    return "('" + ipAddress + "', " + to_string(ntohs(port)) + ")";
}

string User::PortAndIpAndNameToString() const {
    return "[" + PortAndIpIntoString() + ":" + userName + "]"; 
}

bool User::operator<(const User& other) const {
    return socketNumber < other.socketNumber;
}

string User::GetUserName() const {
    if (userName != "None")
        return userName;
    return PortAndIpIntoString();
}