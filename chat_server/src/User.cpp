#include "../include/User.h"

using namespace std;

User::User() {
    userName = "None";
}

User::~User() {}

string User::PortAndIpIntoString() {
    return "(" + ipAddress + ", " + to_string(ntohs(port)) + ")";
}

string User::PortAndIpAndNameToString() {
    return "[" + PortAndIpIntoString() + ":" + userName + "]"; 
}

bool User::operator<(const User& other) const {
    return socketNumber < other.socketNumber;
}